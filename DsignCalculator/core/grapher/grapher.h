#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "../tokenizer/tokenizer.h"
#include <atomic>
#include <unordered_map>

void drawGraph(sf::RenderWindow& window, const std::string& expr, const sf::Font& font);
std::vector<sf::Vertex> computeGraph(const std::string& expr, sf::Color color = sf::Color::Cyan, double scale = 50.0, double xMin = -8.0, double xMax = 8.0, double step = 0.01, double centerX = 400.0, double centerY = 300.0);
// updated: provide screenHeight so implicit 2D contour plotting can compute y range
std::vector<sf::Vertex> computeGraphFromRPN(const std::vector<Token>& rpn, sf::Color color = sf::Color::Cyan, double scale = 50.0, double xMin = -8.0, double xMax = 8.0, double step = 0.01, double centerX = 400.0, double centerY = 300.0, int screenWidth = 0, int screenHeight = 0, const std::unordered_map<std::string,double>* env = nullptr, std::atomic<bool>* cancel = nullptr);
std::vector<sf::Vector2f> computeWorldSamplesFromRPN(const std::vector<Token>& rpn, double xMin = -8.0, double xMax = 8.0, double step = 0.01, const std::unordered_map<std::string,double>* env = nullptr);
