#include <SFML/Graphics.hpp>
#include "../core/grapher/grapher.h"
#include "../core/parser/core_parser.h"
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>

static std::string normalizeExpression(const std::string &in) {
    std::string s = in;
    auto replaceAll = [&](const std::string &a, const std::string &b) {
        size_t p = 0;
        while ((p = s.find(a, p)) != std::string::npos) {
            s.replace(p, a.size(), b);
            p += b.size();
        }
    };
    // map common Unicode symbols to ASCII equivalents
    replaceAll("\xCF\x80", "pi");   // π
    replaceAll("\xCE\xA6", "phi");  // Φ
    replaceAll("\xCF\x86", "phi");  // φ
    replaceAll("\xC2\xB7", "*");    // ·
    replaceAll("\xC3\x97", "*");    // ×
    replaceAll("\xE2\x88\x92", "-"); // − (unicode minus)

    // convert equations of form lhs = rhs into (lhs)-(rhs) so implicit contour F(x,y)=0 is plotted
    size_t eq = s.find('=');
    if (eq != std::string::npos) {
        std::string lhs = s.substr(0, eq);
        std::string rhs = s.substr(eq + 1);
        // trim
        auto trim = [](std::string &str) {
            size_t a = 0; while (a < str.size() && isspace((unsigned char)str[a])) ++a;
            size_t b = str.size(); while (b > a && isspace((unsigned char)str[b-1])) --b;
            str = str.substr(a, b - a);
        };
        trim(lhs); trim(rhs);
        s = std::string("(") + lhs + ")-(" + rhs + ")";
    }
    return s;
}

static void parseParamAssignment(const std::string &line, std::unordered_map<std::string,double> &env) {
    // parse single "name=value" assignment and update env
    size_t eq = line.find('=');
    if (eq == std::string::npos) return;
    std::string name = line.substr(0, eq);
    std::string val = line.substr(eq + 1);
    auto trim = [](std::string &str) {
        size_t a = 0; while (a < str.size() && isspace((unsigned char)str[a])) ++a;
        size_t b = str.size(); while (b > a && isspace((unsigned char)str[b-1])) --b;
        str = str.substr(a, b - a);
    };
    trim(name); trim(val);
    if (name.empty() || val.empty()) return;
    try {
        double d = std::stod(val);
        env[name] = d;
    } catch (...) {
        // ignore parse errors
    }
}

int main() {
    sf::RenderWindow window(sf::VideoMode(1000, 800), "Graphing Calculator");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        font.loadFromFile("C:\\Windows\\Fonts\\arial.ttf");
    }

    const int INITIAL_INPUTS = 3;
    const int INITIAL_PARAMS = 2;
    const int MAX_INPUTS = 15;
    const int MAX_PARAMS = 15;

    // Sidebar width (input area) on the right
    const float sidebarWidth = 400.f; // space reserved on right
    const float sidebarPadding = 10.f;

    std::vector<std::string> currentInput;
    std::vector<std::vector<sf::Vertex>> lastGraph;
    std::vector<std::string> lastExpr;
    std::vector<std::vector<Token>> lastRPN;
    std::vector<double> lastCenterX;
    std::vector<double> lastCenterY;
    std::vector<sf::Color> colors;
    std::vector<sf::Text> inputTexts;

    std::unordered_map<std::string,double> env; // parameter environment

    // parameter input boxes (left column of sidebar) — act like function inputs
    std::vector<std::string> paramInputs;
    std::vector<sf::Text> paramInputTexts;
    int activeParam = -1; // which param box is focused for typing

    // parameter editing helper: show parsed values in env when committed

    std::vector<sf::Color> palette = {
        sf::Color::Cyan, sf::Color::Magenta, sf::Color::Yellow, sf::Color::Red,
        sf::Color::Green, sf::Color::Blue, sf::Color(255,128,0), sf::Color(128,0,255),
        sf::Color(0,200,200), sf::Color(200,0,200), sf::Color(200,200,0), sf::Color(0,150,0),
        sf::Color(0,0,150), sf::Color(150,0,0), sf::Color(100,100,100)
    };

    auto addInputBox = [&](const std::string& initText = std::string()) {
        if ((int)currentInput.size() >= MAX_INPUTS) return;
        int i = (int)currentInput.size();
        currentInput.push_back(initText);
        lastGraph.emplace_back();
        lastExpr.emplace_back();
        lastRPN.emplace_back();
        lastCenterX.push_back(0.0);
        lastCenterY.push_back(0.0);
        colors.push_back(palette[i % (int)palette.size()]);
        sf::Text t;
        t.setFont(font);
        t.setCharacterSize(16);
        t.setFillColor(sf::Color::White);
        inputTexts.push_back(t);
    };

    auto addParamBox = [&](const std::string& initText = std::string()) {
        if ((int)paramInputs.size() >= MAX_PARAMS) return;
        paramInputs.push_back(initText);
        sf::Text t; t.setFont(font); t.setCharacterSize(14); t.setFillColor(sf::Color::White);
        paramInputTexts.push_back(t);
    };

    for (int i = 0; i < INITIAL_INPUTS; ++i) addInputBox();
    for (int i = 0; i < INITIAL_PARAMS; ++i) addParamBox();

    int active = 0;
    bool needRedraw = true;

    double scale = 50.0;
    const double MIN_SCALE = 1.0;   // change as desired
    const double MAX_SCALE = 4000.0; // change as desired
    auto computeAdaptiveStep = [](double s) {
        return std::max(0.001, 1.0 / s * 0.5);
    };

    // synchronous recompute function: compute all graphs immediately on the main thread
    auto computeAllGraphs = [&](double centerX, double centerY) {
        double step = computeAdaptiveStep(scale);
        int graphW = (int)(window.getSize().x - (int)sidebarWidth);
        int screenW = graphW;
        int screenH = window.getSize().y;
        // compute world x range based on center pixel (consistent with drawing)
        double xMin = (0.0 - centerX) / scale;
        double xMax = ((double)screenW - centerX) / scale;

        // ensure vectors are sized correctly
        size_t n = std::max(lastRPN.size(), currentInput.size());
        if (lastGraph.size() < n) lastGraph.resize(n);
        if (lastCenterX.size() < n) lastCenterX.resize(n, centerX);
        if (lastCenterY.size() < n) lastCenterY.resize(n, centerY);

        for (size_t i = 0; i < lastRPN.size(); ++i) {
            if (lastRPN[i].empty()) { lastGraph[i].clear(); continue; }
            lastGraph[i] = computeGraphFromRPN(lastRPN[i], colors[i % colors.size()], scale, xMin, xMax, step, centerX, centerY, screenW, screenH, &env);
            lastCenterX[i] = centerX;
            lastCenterY[i] = centerY;
        }
    };

    bool dragging = false;
    sf::Vector2i dragStart(0,0);
    double panX = 0.0, panY = 0.0;
    double panStartX = 0.0, panStartY = 0.0;

    // when dragging we avoid recomputing; schedule a recompute on release or after idle
    bool pendingComputeAfterDrag = false;
    sf::Clock dragIdleClock;
    const sf::Time dragIdleThreshold = sf::milliseconds(200);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
                break;
            }

            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2i mpos = sf::Mouse::getPosition(window);
                    int graphW = window.getSize().x - (int)sidebarWidth;

                    // layout: left column in sidebar for params, right column for inputs
                    int sidebarLeft = window.getSize().x - (int)sidebarWidth;
                    int paramColW = (int)(sidebarWidth * 0.35f); // smaller param column
                    int paramX = sidebarLeft + 0 + (int)sidebarPadding;
                    int inputColX = sidebarLeft + paramColW + (int)sidebarPadding;

                    // check clicks in param column
                    int paramY = (int)sidebarPadding;
                    int paramW = paramColW - (int)sidebarPadding*2;
                    int paramH = 24;
                    // header click makes first param active
                    if (mpos.x >= graphW && mpos.x >= paramX && mpos.x <= paramX + paramW && mpos.y >= paramY && mpos.y <= paramY + paramH) {
                        if (!paramInputs.empty()) activeParam = 0;
                        needRedraw = true;
                        continue;
                    }

                    // check param input boxes
                    int pvBaseY = paramY + paramH + 6;
                    for (size_t pi = 0; pi < paramInputs.size(); ++pi) {
                        int sy = pvBaseY + (int)pi * 26;
                        int valX = paramX + 6;
                        int valW = paramW - 12;
                        if (mpos.x >= valX && mpos.x <= valX + valW && mpos.y >= sy && mpos.y <= sy + 20) {
                            activeParam = (int)pi;
                            needRedraw = true;
                            break;
                        }
                    }
                    if (activeParam != -1) {
                        // if the click is in the input column, allow switching to graph inputs
                        if (mpos.x >= inputColX) {
                            activeParam = -1; // clear param focus so input column handling can take place
                        } else {
                            continue;
                        }
                    }

                    // check Add button in input column (right column)
                    int addX = inputColX + 6;
                    int addY = (int)sidebarPadding;
                    int addW = 24; int addH = 24;
                    if (mpos.x >= addX && mpos.x <= addX + addW && mpos.y >= addY && mpos.y <= addY + addH) {
                        addInputBox(); needRedraw = true; continue;
                    }

                    // if click in input column, focus proper input
                    if (mpos.x >= graphW && mpos.x >= inputColX) {
                        int localY = mpos.y - (int)sidebarPadding - 32; // leave space for param area
                        if (localY >= 0) {
                            int idx = localY / 26; // line spacing
                            if (idx >= 0 && idx < (int)currentInput.size()) {
                                active = idx;
                                needRedraw = true;
                            }
                        }
                    }
                } else if (event.mouseButton.button == sf::Mouse::Right) {
                    // start panning only when right-click in graph area
                    sf::Vector2i mpos = sf::Mouse::getPosition(window);
                    int graphW = window.getSize().x - (int)sidebarWidth;
                    if (mpos.x < graphW) {
                        dragging = true;
                        dragStart = sf::Mouse::getPosition(window);
                        panStartX = panX; panStartY = panY;
                        pendingComputeAfterDrag = false;
                    }
                }
            }

            if (event.type == sf::Event::MouseButtonReleased) {
                if (event.mouseButton.button == sf::Mouse::Right) {
                    if (dragging) {
                        dragging = false;
                        int graphW = window.getSize().x - (int)sidebarWidth;
                        float centerX = graphW / 2.0f + (float)panX;
                        float centerY = (float)window.getSize().y / 2.0f + (float)panY;
                        computeAllGraphs(centerX, centerY);
                        needRedraw = true;
                    }
                }
            }

            if (event.type == sf::Event::MouseMoved) {
                sf::Vector2i mpos = sf::Mouse::getPosition(window);
                if (dragging) {
                    panX = panStartX + (mpos.x - dragStart.x);
                    panY = panStartY + (mpos.y - dragStart.y);
                    pendingComputeAfterDrag = true;
                    dragIdleClock.restart();
                    needRedraw = true;
                }
            }

            if (event.type == sf::Event::MouseWheelScrolled) {
                // Update scale immediately and recompute; only if mouse over graph area
                sf::Vector2i mpos = sf::Mouse::getPosition(window);
                int graphW = window.getSize().x - (int)sidebarWidth;
                if (mpos.x < graphW) {
                    if (event.mouseWheelScroll.delta > 0) scale *= 1.12;
                    else scale /= 1.12;
                    if (scale < MIN_SCALE) scale = MIN_SCALE;
                    if (scale > MAX_SCALE) scale = MAX_SCALE;
                    float centerX = graphW / 2.0f + panX;
                    float centerY = window.getSize().y / 2.0f + panY;
                    computeAllGraphs(centerX, centerY);
                    needRedraw = true;
                }
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Up) {
                    active = (active - 1 + (int)currentInput.size()) % (int)currentInput.size();
                    needRedraw = true;
                }
                if (event.key.code == sf::Keyboard::Down) {
                    active = (active + 1) % (int)currentInput.size();
                    needRedraw = true;
                }
            }

            if (event.type == sf::Event::TextEntered) {
                uint32_t code = event.text.unicode;
                // parameter input boxes handling
                if (activeParam != -1) {
                    if (code == 8) { if (!paramInputs[activeParam].empty()) paramInputs[activeParam].pop_back(); needRedraw = true; }
                    else if (code == 13) {
                        // commit param line -> parse name=value
                        parseParamAssignment(paramInputs[activeParam], env);
                        // optionally add next param box if this was the last
                        bool allFilled = true;
                        for (auto &s : paramInputs) if (s.empty()) { allFilled = false; break; }
                        if (allFilled && (int)paramInputs.size() < MAX_PARAMS) addParamBox();
                        activeParam = -1;
                        // recompute graphs
                        int graphW = window.getSize().x - (int)sidebarWidth;
                        float centerX = graphW / 2.0f + (float)panX;
                        float centerY = (float)window.getSize().y / 2.0f + (float)panY;
                        computeAllGraphs(centerX, centerY);
                        needRedraw = true;
                    }
                    else if (code < 128) { paramInputs[activeParam] += static_cast<char>(code); needRedraw = true; }
                    continue;
                }

                if (code == 8) {
                    if (!currentInput[active].empty()) currentInput[active].pop_back();
                    needRedraw = true;
                }
                else if (code == 13) {
                    int graphW = window.getSize().x - (int)sidebarWidth;
                    int graphH = window.getSize().y;
                    float centerX = graphW / 2.0f + panX;
                    float centerY = graphH / 2.0f + panY;
                    double xRange = (graphW / 2.0) / scale;
                    double xMin = -xRange;
                    double xMax = xRange;
                    double step = computeAdaptiveStep(scale);

                    try {
                        std::string expr = normalizeExpression(currentInput[active]);
                        auto tokens = tokenize(expr);
                        auto rpn = shuntingYard(tokens);
                        auto graph = computeGraphFromRPN(rpn, colors[active], scale, xMin, xMax, step, centerX, centerY, graphW, graphH, &env);
                        if (!graph.empty()) {
                            lastGraph[active] = std::move(graph);
                            lastExpr[active] = currentInput[active];
                            lastRPN[active] = std::move(rpn);
                            lastCenterX[active] = centerX;
                            lastCenterY[active] = centerY;
                        } else {
                            std::cerr << "Expression produced no points or was invalid for input " << (active+1) << ". Keeping previous graph.\n";
                        }
                    }
                    catch (...) {
                        std::cerr << "Parse/eval error for input " << (active+1) << "\n";
                    }

                    needRedraw = true;
                }
                else if (code < 128) {
                    currentInput[active] += static_cast<char>(code);
                    needRedraw = true;
                }

                bool allFilled = true;
                for (auto &s : currentInput) if (s.empty()) { allFilled = false; break; }
                if (allFilled && (int)currentInput.size() < MAX_INPUTS) {
                    addInputBox();
                    needRedraw = true;
                }
            }
        }

        // if dragging stopped and pending recompute, do it after idle
        if (pendingComputeAfterDrag && !dragging && dragIdleClock.getElapsedTime() >= dragIdleThreshold) {
            int graphW = window.getSize().x - (int)sidebarWidth;
            int graphH = window.getSize().y;
            float centerX = graphW / 2.0f + (float)panX;
            float centerY = (float)window.getSize().y / 2.0f + (float)panY;
            computeAllGraphs(centerX, centerY);
            pendingComputeAfterDrag = false; needRedraw = true;
        }

        if (!needRedraw) {
            sf::sleep(sf::milliseconds(10));
            continue;
        }

        window.clear(sf::Color::Black);

        int graphW = window.getSize().x - (int)sidebarWidth;
        int graphH = window.getSize().y;
        float centerX = graphW / 2.0f + (float)panX;
        float centerY = (float)graphH / 2.0f + (float)panY;

        sf::Vertex xAxis[] = { {{0.f, centerY},sf::Color::White}, {{(float)graphW, centerY},sf::Color::White} };
        sf::Vertex yAxis[] = { {{centerX,0.f},sf::Color::White}, {{centerX, (float)graphH},sf::Color::White} };
        window.draw(xAxis, 2, sf::Lines);
        window.draw(yAxis, 2, sf::Lines);

        double xMinWorld = (0.0 - centerX) / scale;
        double xMaxWorld = ((double)graphW - centerX) / scale;
        double yMaxWorld = centerY / scale;
        double yMinWorld = (centerY - graphH) / scale;

        // adaptive tick spacing
        double pixelsPerUnit = scale;
        double minPixelSpacing = 60.0;
        double rawSpacing = minPixelSpacing / pixelsPerUnit;
        if (rawSpacing <= 0) rawSpacing = 1.0;
        double pow10 = std::pow(10.0, std::floor(std::log10(rawSpacing)));
        double n = rawSpacing / pow10;
        double tickSpacing;
        if (n <= 1.0) tickSpacing = 1.0 * pow10;
        else if (n <= 2.0) tickSpacing = 2.0 * pow10;
        else if (n <= 5.0) tickSpacing = 5.0 * pow10;
        else tickSpacing = 10.0 * pow10;

        double xFirst = std::ceil(xMinWorld / tickSpacing) * tickSpacing;
        for (double xv = xFirst; xv <= xMaxWorld + 1e-9; xv += tickSpacing) {
            float px = static_cast<float>(centerX + xv * scale);
            sf::Vertex tick[] = { {{px, centerY - 5.f}, sf::Color::White}, {{px, centerY + 5.f}, sf::Color::White} };
            window.draw(tick, 2, sf::Lines);
            if (font.getInfo().family != "") {
                int decimals = 0;
                if (tickSpacing < 1.0) decimals = (int)std::ceil(-std::log10(tickSpacing));
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(decimals) << xv;
                sf::Text t(ss.str(), font, 12);
                t.setFillColor(sf::Color::White);
                t.setPosition(px + 2.f, centerY + 6.f);
                window.draw(t);
            }
        }

        double yFirst = std::ceil(yMinWorld / tickSpacing) * tickSpacing;
        for (double yv = yFirst; yv <= yMaxWorld + 1e-9; yv += tickSpacing) {
            float py = static_cast<float>(centerY - yv * scale);
            sf::Vertex tick[] = { {{centerX - 5.f, py}, sf::Color::White}, {{centerX + 5.f, py}, sf::Color::White} };
            window.draw(tick, 2, sf::Lines);
            if (font.getInfo().family != "") {
                int decimals = 0;
                if (tickSpacing < 1.0) decimals = (int)std::ceil(-std::log10(tickSpacing));
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(decimals) << yv;
                sf::Text t(ss.str(), font, 12);
                t.setFillColor(sf::Color::White);
                t.setPosition(centerX + 6.f, py - 8.f);
                window.draw(t);
            }
        }

        // draw cached graphs (translated during pan)
        for (size_t i = 0; i < lastGraph.size(); ++i) {
            if (!lastGraph[i].empty()) {
                float dx = (float)(centerX - lastCenterX[i]);
                float dy = (float)(centerY - lastCenterY[i]);
                sf::RenderStates states;
                states.transform.translate(dx, dy);
                window.draw(&lastGraph[i][0], lastGraph[i].size(), sf::LineStrip, states);
            }
        }

        // draw sidebar background
        sf::RectangleShape sideBg(sf::Vector2f(sidebarWidth, (float)window.getSize().y));
        sideBg.setPosition((float)window.getSize().x - sidebarWidth, 0.f);
        sideBg.setFillColor(sf::Color(22,22,22));
        window.draw(sideBg);

        // draw param input box at top of sidebar (left column)
        int sidebarLeft = window.getSize().x - (int)sidebarWidth;
        int paramColW = (int)(sidebarWidth * 0.35f); // smaller param column
        int paramX = sidebarLeft + 0 + (int)sidebarPadding;
        int inputColX = sidebarLeft + paramColW + (int)sidebarPadding;
        {
            float bx = (float)paramX;
            float by = (float)sidebarPadding;
            float bw = (float)(paramColW - sidebarPadding*2);
            float bh = 24.f;
            sf::RectangleShape box(sf::Vector2f(bw, bh)); box.setPosition(bx, by);
            box.setFillColor(sf::Color(40,40,40));
            box.setOutlineThickness(1.f); box.setOutlineColor(sf::Color(80,80,80)); window.draw(box);
            sf::Text t; t.setFont(font); t.setCharacterSize(14); t.setFillColor(sf::Color::White);
            t.setString(std::string("params: "));
            t.setPosition(bx+6.f, by+3.f); window.draw(t);
        }

        // draw parameter text boxes (left column)
        int paramW = paramColW - (int)sidebarPadding*2;
        int pvBaseY = sidebarPadding + 24 + 6;
        for (size_t pi = 0; pi < paramInputs.size(); ++pi) {
            std::string name = "param" + std::to_string(pi+1);
            double val = env[name];
            float y = (float)(pvBaseY + pi * 28);
            // name
            sf::Text tn; tn.setFont(font); tn.setCharacterSize(14); tn.setFillColor(sf::Color::White);
            tn.setString(name + ":"); tn.setPosition((float)paramX + 6.f, y - 2.f); window.draw(tn);
            // value box
            float valX = (float)paramX + 70.f;
            float valW = (float)(paramW - 80);
            sf::RectangleShape vbox(sf::Vector2f(valW, 20.f)); vbox.setPosition(valX, y); vbox.setFillColor(activeParam == (int)pi ? sf::Color(60,60,60) : sf::Color(40,40,40)); vbox.setOutlineThickness(1.f); vbox.setOutlineColor(sf::Color(80,80,80)); window.draw(vbox);
            // value text
            std::string s = paramInputs[pi];
            sf::Text tv; tv.setFont(font); tv.setCharacterSize(12); tv.setFillColor(sf::Color::White); tv.setString(s); tv.setPosition(valX + 4.f, y + 2.f); window.draw(tv);
        }

        // draw Add (+) button in input column
        {
            float bx = (float)inputColX + 6.f;
            float by = (float)sidebarPadding;
            sf::RectangleShape addBox(sf::Vector2f(24.f, 24.f)); addBox.setPosition(bx, by); addBox.setFillColor(sf::Color(60,60,60)); addBox.setOutlineThickness(1.f); addBox.setOutlineColor(sf::Color(120,120,120)); window.draw(addBox);
            sf::Text plus; plus.setFont(font); plus.setCharacterSize(18); plus.setFillColor(sf::Color::White); plus.setString("+"); plus.setPosition(bx+6.f, by); window.draw(plus);
        }

        // draw inputTexts in input column (give more horizontal space)
        for (size_t i = 0; i < inputTexts.size(); ++i) {
            float x = (float)inputColX + 6.f; // more left space
            float y = 10.f + 32.f + i * 26.f;
            inputTexts[i].setPosition(x, y);
            std::string label = std::to_string(i+1) + ": f(x) = " + currentInput[i];
            inputTexts[i].setString(label);
            if ((int)i == active) inputTexts[i].setFillColor(sf::Color::Green);
            else inputTexts[i].setFillColor(sf::Color::White);
            if (font.getInfo().family != "") window.draw(inputTexts[i]);
        }

        for (size_t i = 0; i < lastExpr.size(); ++i) {
            if (!lastExpr[i].empty()) {
                sf::Text t(lastExpr[i], font, 14);
                t.setFillColor(colors[i % colors.size()]);
                // place colored legend on the left side of the graph area
                float legendX = 10.f;
                float legendY = 10.f + i * 18.f;
                t.setPosition(legendX, legendY);
                if (font.getInfo().family != "") window.draw(t);
            }
        }

        window.display();
        needRedraw = false;
    }

    return 0;
}
