#include <algorithm>
#include <cstdlib>
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include <sys/ioctl.h>
#include <unistd.h>

using json = nlohmann::json;

struct Candle {
    std::string date;
    double open;
    double high;
    double low;
    double close;
};

std::string getApiKey() {
    const char *homeDir = std::getenv("HOME");
    if (!homeDir) {
        homeDir = std::getenv("USERPROFILE");
    }

    std::string path = std::string(homeDir) + "/.stock_api_key";
    std::string key;

    std::ifstream inFile(path);
    if (inFile.is_open()) {
        std::getline(inFile, key);
        inFile.close();

        key.erase(0, key.find_first_not_of(" \t\n\r"));
        key.erase(key.find_last_not_of(" \t\n\r") + 1);

        if (!key.empty()) {
            return key;
        }
    }

    std::cout << "First time setup: Please enter your Alpha Vantage API Key.\n";
    std::cout << "(It will be saved to " << path << ")\n";
    std::cout << "Key: ";
    std::cin >> key;

    // 4. Save the key to the file
    std::ofstream outFile(path);
    if (outFile.is_open()) {
        outFile << key;
        outFile.close();
        std::cout << "Key saved successfully.\n\n";
    } else {
        std::cerr << "Warning: Could not save API key to file.\n";
    }

    return key;
}

size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                     std::string *userp) {
    size_t totalSize = size * nmemb;
    userp->append((char *)contents, totalSize);
    return totalSize;
}

std::string fetchData(const std::string &symbol, const std::string &apiKey) {
    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        std::string url = "https://www.alphavantage.co/"
                          "query?function=TIME_SERIES_DAILY&symbol=" +
                          symbol + "&apikey=" + apiKey;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return readBuffer;
}

class Chart {
    int height;

  public:
    Chart(int h) : height(h) {}

    int getTerminalWidth() {
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
            return 80;
        }
        return w.ws_col;
    }

    void draw(const std::vector<Candle> &candles, std::string &symbol) {
        if (candles.empty())
            return;

        int termWidth = getTerminalWidth();
        int padding = 4;
        int availableCols = termWidth - padding;

        int stride = 2;
        int maxCandles = availableCols / stride;

        int numCandlesToShow = std::min((int)candles.size(), maxCandles);
        int startIdx = candles.size() - numCandlesToShow;

        std::vector<Candle> window(candles.begin() + startIdx, candles.end());

        double minPrice = window[0].low;
        double maxPrice = window[0].high;

        for (const auto &c : window) {
            if (c.low < minPrice)
                minPrice = c.low;
            if (c.high > maxPrice)
                maxPrice = c.high;
        }
        if (maxPrice == minPrice)
            maxPrice += 1.0;

        std::vector<std::string> screen(height,
                                        std::string(window.size(), ' '));

        for (int col = 0; col < window.size(); ++col) {
            const auto &c = window[col];
            bool isBullish = (c.close >= c.open);

            auto getRow = [&](double price) {
                double ratio = (price - minPrice) / (maxPrice - minPrice);
                int row = (int)(ratio * (height - 1) + 0.5);
                return (height - 1) - row;
            };

            int yHigh = getRow(c.high);
            int yLow = getRow(c.low);
            int yOpen = getRow(c.open);
            int yClose = getRow(c.close);

            int bodyTop = std::min(yOpen, yClose);
            int bodyBottom = std::max(yOpen, yClose);

            for (int r = yHigh; r <= yLow; ++r) {
                screen[r][col] = '|';
            }

            char symbol = isBullish ? 'O' : '#';
            for (int r = bodyTop; r <= bodyBottom; ++r) {
                screen[r][col] = symbol;
            }
        }

        std::cout << "\n\033[1m" << "Chart: " << symbol << " (" << window.size()
                  << " candles)" << "\033[0m" << std::endl;
        std::cout << "Max: " << maxPrice << "\n" << std::endl;

        for (int r = 0; r < height; ++r) {
            std::cout << "  ";
            for (char cell : screen[r]) {
                if (cell == 'O')
                    std::cout << "\033[32m" << "█" << "\033[0m ";
                else if (cell == '#')
                    std::cout << "\033[31m" << "█" << "\033[0m ";
                else if (cell == '|')
                    std::cout << "\033[90m" << "│" << "\033[0m ";
                else
                    std::cout << "  ";
            }
            std::cout << "\n";
        }

        std::cout << "\nMin: " << minPrice << std::endl;
        std::cout << "Range: " << window.front().date << " -> "
                  << window.back().date << std::endl;
        std::cout << std::endl;
    }
};

int main(int argc, char **argv) {
    std::string apiKey = getApiKey();
    std::string symbol = "AAPL";

    if (argc > 1) {
        symbol = argv[1];
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::cout << "Fetching " << symbol << "..." << std::endl;
    std::string jsonStr = fetchData(symbol, apiKey);

    std::vector<Candle> candles;
    try {
        auto jsonData = json::parse(jsonStr);
        if (jsonData.contains("Error Message")) {
            std::cerr << "API Error." << std::endl;
            return 1;
        }

        auto timeSeries = jsonData["Time Series (Daily)"];
        for (auto it = timeSeries.begin(); it != timeSeries.end(); ++it) {
            Candle c;
            c.date = it.key();
            c.open = std::stod(it.value()["1. open"].get<std::string>());
            c.high = std::stod(it.value()["2. high"].get<std::string>());
            c.low = std::stod(it.value()["3. low"].get<std::string>());
            c.close = std::stod(it.value()["4. close"].get<std::string>());
            candles.push_back(c);
        }
    } catch (std::exception &e) {
        std::cerr << "Parse Error: " << e.what() << std::endl;
        return 1;
    }

    Chart chart(20);
    chart.draw(candles, symbol);
    curl_global_cleanup();
    return 0;
}
