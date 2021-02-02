#pragma once

#include <algorithm> //reverse
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring> //memcpy
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "config.hpp"
#include "kiteppexceptions.hpp"
#include "responses.hpp"

#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "rjhelper.hpp"
#include "uWS.h"

namespace kitepp {

// To make sure doubles are parsed correctly
static_assert(std::numeric_limits<double>::is_iec559, "Requires IEEE 754 floating point!");

using std::string;

namespace rj = rapidjson;
namespace rjh = RJHelper;

class kiteWS {

  public:
    // member variables
    // user constants
    const string MODE_LTP = "ltp";
    const string MODE_QUOTE = "quote";
    const string MODE_FULL = "full";

    // callbacks
    std::function<void(kiteWS* ws)> onConnect;
    std::function<void(kiteWS* ws, const std::vector<kitepp::tick>& ticks)> onTicks;
    std::function<void(kiteWS* ws, const kitepp::postback& postback)> onOrderUpdate;
    std::function<void(kiteWS* ws, const string& message)> onMessage;
    std::function<void(kiteWS* ws, int code, const string& message)> onError;
    std::function<void(kiteWS* ws)> onWSError;
    std::function<void(kiteWS* ws, int code, const string& message)> onClose;

    // constructors & destructors

    kiteWS(const string& apiKey)
        : _apiKey(apiKey), _hubGroup(_hub.createGroup<uWS::CLIENT>()) {

          };

    ~kiteWS() {

    };

    // methods

    /**
     * @brief Set the API key
     *
     * @param arg
     */
    void setAPIKey(const string& arg) { _apiKey = arg; };

    /**
     * @brief get set API key
     *
     * @return string
     */
    string getAPIKey() const { return _apiKey; };

    /**
     * @brief Set the Access Token
     *
     * @param arg the string you want to set as access token
     *
     * @paragraph ex1 example
     * @snippet example2.cpp settting access token
     */
    void setAccessToken(const string& arg) { _accessToken = arg; };

    /**
     * @brief Get the Access Token set currently
     *
     * @return string
     */
    string getAccessToken() const { return _accessToken; };

    void connect() {

        _assignCallbacks();
        _hub.connect(FMT(_connectURLFmt, _apiKey, _accessToken), nullptr, {}, 5000, _hubGroup);
    };

    bool isConnected() { return _WS; };

    std::chrono::time_point<std::chrono::system_clock> getLastBeatTime() { return _lastBeatTime; };

    void run() {
        _pingThread = std::thread(&kiteWS::_pingLoop, this);
        _hub.run();
    };

    void stop() {

        _stop = true;
        if (_pingThread.joinable()) { _pingThread.join(); };
        _WS->close();
    };

    void subscribe(const std::vector<int>& instrumentToks) {

        rj::Document req;
        req.SetObject();
        auto& reqAlloc = req.GetAllocator();
        rj::Value val;
        rj::Value toksArr(rj::kArrayType);

        val.SetString("subscribe", reqAlloc);
        req.AddMember("a", val, reqAlloc);

        for (const int& tok : instrumentToks) { toksArr.PushBack(tok, reqAlloc); }
        req.AddMember("v", toksArr, reqAlloc);

        string reqStr = rjh::_dump(req);
        (_WS) ? _WS->send(reqStr.data(), reqStr.size(), uWS::OpCode::TEXT) :
                throw kitepp::libException("_WS doesn't point to anything");
    };

    void unsubscribe(const std::vector<int>& instrumentToks) {

        rj::Document req;
        req.SetObject();
        auto& reqAlloc = req.GetAllocator();
        rj::Value val;
        rj::Value toksArr(rj::kArrayType);

        val.SetString("unsubscribe", reqAlloc);
        req.AddMember("a", val, reqAlloc);

        for (const int& tok : instrumentToks) { toksArr.PushBack(tok, reqAlloc); }
        req.AddMember("v", toksArr, reqAlloc);

        string reqStr = rjh::_dump(req);
        (_WS) ? _WS->send(reqStr.data(), reqStr.size(), uWS::OpCode::TEXT) :
                throw kitepp::libException("_WS doesn't point to anything");
    };

    void setMode(const string& mode, const std::vector<int>& instrumentToks) {
        // FIXME check if string produced in correct

        rj::Document req;
        req.SetObject();
        auto& reqAlloc = req.GetAllocator();
        rj::Value val;
        rj::Value valArr(rj::kArrayType);
        rj::Value toksArr(rj::kArrayType);

        val.SetString("mode", reqAlloc);
        req.AddMember("a", val, reqAlloc);

        val.SetString(mode.c_str(), mode.size(), reqAlloc);
        valArr.PushBack(val, reqAlloc);
        for (const int& tok : instrumentToks) { toksArr.PushBack(tok, reqAlloc); }
        valArr.PushBack(toksArr, reqAlloc);
        req.AddMember("v", valArr, reqAlloc);

        string reqStr = rjh::_dump(req);
        (_WS) ? _WS->send(reqStr.data(), reqStr.size(), uWS::OpCode::TEXT) :
                throw kitepp::libException("_WS doesn't point to anything");
    };

  private:
    // member variables
    const string _connectURLFmt = "wss://ws.kite.trade/?api_key={0}&access_token={1}";
    string _apiKey;
    string _accessToken;
    const std::unordered_map<string, int> _segmentConstants = {

        { "nse", 1 },
        { "nfo", 2 },
        { "cds", 3 },
        { "bse", 4 },
        { "bfo", 5 },
        { "bsecds", 6 },
        { "mcx", 7 },
        { "mcxsx", 8 },
        { "indices", 9 },
    };

    uWS::Hub _hub;
    uWS::Group<uWS::CLIENT>* _hubGroup;
    uWS::WebSocket<uWS::CLIENT>* _WS;

    std::atomic<bool> _stop { false };
    const string _pingMessage = "";
    std::thread _pingThread;
    const unsigned int _pingInterval = 3; // in seconds

    std::chrono::time_point<std::chrono::system_clock> _lastBeatTime;

    // methods

    void _assignCallbacks() {

        // will need to have a pointer to this class
        _hubGroup->onConnection([&](uWS::WebSocket<uWS::CLIENT>* ws, uWS::HttpRequest req) {
            std::cout << "connected...\n";
            _WS = ws;
            if (onConnect) { onConnect(this); };
        });

        _hubGroup->onMessage([&](uWS::WebSocket<uWS::CLIENT>* ws, char* message, size_t length, uWS::OpCode opCode) {
            if (opCode == uWS::OpCode::BINARY && onTicks) {

                onTicks(this, _parseBinaryMessage(message, length));

            } else if (opCode == uWS::OpCode::TEXT) {

                _parseTextMessage(message, length);
            };
        });

        _hubGroup->onPong([&](uWS::WebSocket<uWS::CLIENT>* ws, char* message, size_t length) {
            std::cout << "Pong recieved..\n";
            // Will probably need to record this time for implementing reconnecting
        });

        _hubGroup->onError([&](void*) {
            if (onWSError) { onWSError(this); }
        });

        _hubGroup->onDisconnection([&](uWS::WebSocket<uWS::CLIENT>* ws, int code, char* reason, size_t length) {
            _WS = nullptr;
            if (code != 1000 && onError) { onError(this, code, string(reason, length)); };
            if (onClose) { onClose(this, code, string(reason, length)); };
        });
    };

    void _parseTextMessage(char* message, size_t length) {

        rj::Document res;
        rjh::_parse(res, string(message, length));
        if (!res.IsObject()) { throw libException("Expected a JSON object"); };

        string type;
        rjh::_getIfExists(res, type, "type");
        if (type.empty()) { throw kitepp::libException(FMT("Cannot recognize websocket message type {0}", type)); }

        if (type == "order" && onOrderUpdate) { onOrderUpdate(this, kitepp::postback(res["data"].GetObject())); }
        if (type == "message" && onMessage) { onMessage(this, string(message, length)); };
        if (type == "error" && onError) { onError(this, 0, string(message, length)); };
    };

    // Convert bytesarray(array[start], arrray[end]) to number of type T
    template <typename T> T _getNum(const std::vector<char>& bytes, size_t start, size_t end) {

        T value;
        std::vector<char> requiredBytes(bytes.begin() + start, bytes.begin() + end + 1);

        // clang-format off
        #ifndef WORDS_BIGENDIAN
        std::reverse(requiredBytes.begin(), requiredBytes.end());
        #endif // !IS_BIG_ENDIAN
        // clang-format on

        std::memcpy(&value, requiredBytes.data(), sizeof(T));

        return value;
    };

    std::vector<std::vector<char>> _splitPackets(const std::vector<char>& bytes) {

        // is a heartbeat
        if (bytes.size() < 2) {

            _lastBeatTime = std::chrono::system_clock::now();
            return {};
        };

        int16_t numberOfPackets = _getNum<int16_t>(bytes, 0, 1);

        std::vector<std::vector<char>> packets;

        unsigned int packetLengthStartIdx = 2;
        for (int i = 1; i <= numberOfPackets; i++) {

            unsigned int packetLengthEndIdx = packetLengthStartIdx + 1;
            int16_t packetLength = _getNum<int16_t>(bytes, packetLengthStartIdx, packetLengthEndIdx);
            packetLengthStartIdx = packetLengthEndIdx + packetLength + 1;
            // FIXME this might be wrong. i.e, an index pudhe mage

            packets.emplace_back(bytes.begin() + packetLengthEndIdx + 1, bytes.begin() + packetLengthStartIdx);
        };

        return packets;
    };

    std::vector<kitepp::tick> _parseBinaryMessage(char* bytes, size_t size) {

        std::vector<std::vector<char>> packets = _splitPackets(std::vector<char>(bytes, bytes + size));
        if (packets.empty()) { return {}; };

        std::vector<kitepp::tick> ticks;
        for (const auto& packet : packets) {

            size_t packetSize = packet.size();
            int32_t instrumentToken = _getNum<int32_t>(packet, 0, 3);
            int segment = instrumentToken & 0xff;
            double divisor = (segment == _segmentConstants.at("cds")) ? 10000000.0 : 100.0;
            bool tradable = (segment == _segmentConstants.at("indices")) ? false : true;

            kitepp::tick Tick;

            Tick.isTradable = tradable;
            Tick.instrumentToken = instrumentToken;

            // LTP packet
            if (packetSize == 8) {

                Tick.mode = MODE_LTP;
                Tick.lastPrice = _getNum<int32_t>(packet, 4, 7) / divisor;

            } else if (packetSize == 28 || packetSize == 32) {
                // indices quote and full mode

                Tick.mode = (packetSize == 28) ? MODE_QUOTE : MODE_FULL;
                Tick.lastPrice = _getNum<int32_t>(packet, 4, 7) / divisor;
                Tick.OHLC.high = _getNum<int32_t>(packet, 8, 11) / divisor;
                Tick.OHLC.low = _getNum<int32_t>(packet, 12, 15) / divisor;
                Tick.OHLC.open = _getNum<int32_t>(packet, 16, 19) / divisor;
                Tick.OHLC.close = _getNum<int32_t>(packet, 20, 23) / divisor;
                // xTick.netChange = (Tick.lastPrice - Tick.OHLC.close) * 100 / Tick.OHLC.close;
                Tick.netChange = _getNum<int32_t>(packet, 24, 27) / divisor;

                // parse full mode with timestamp
                if (packetSize == 32) { Tick.timestamp = _getNum<int32_t>(packet, 28, 33); }

            } else if (packetSize == 44 || packetSize == 184) {
                // Quote and full mode

                Tick.mode = (packetSize == 44) ? MODE_QUOTE : MODE_FULL;
                Tick.lastPrice = _getNum<int32_t>(packet, 4, 7) / divisor;
                Tick.lastTradedQuantity = _getNum<int32_t>(packet, 8, 11) / divisor;
                Tick.averageTradePrice = _getNum<int32_t>(packet, 12, 15) / divisor;
                Tick.volumeTraded = _getNum<int32_t>(packet, 16, 19) / divisor;
                Tick.totalBuyQuantity = _getNum<int32_t>(packet, 20, 23) / divisor;
                Tick.totalSellQuantity = _getNum<int32_t>(packet, 24, 27) / divisor;
                Tick.OHLC.high = _getNum<int32_t>(packet, 28, 31) / divisor;
                Tick.OHLC.low = _getNum<int32_t>(packet, 32, 35) / divisor;
                Tick.OHLC.open = _getNum<int32_t>(packet, 36, 39) / divisor;
                Tick.OHLC.close = _getNum<int32_t>(packet, 40, 43) / divisor;

                Tick.netChange = (Tick.lastPrice - Tick.OHLC.close) * 100 / Tick.OHLC.close;

                // parse full mode
                if (packetSize == 184) {

                    Tick.lastTradeTime = _getNum<int32_t>(packet, 44, 47) / divisor;
                    Tick.OI = _getNum<int32_t>(packet, 48, 51) / divisor;
                    Tick.OIDayHigh = _getNum<int32_t>(packet, 52, 55) / divisor;
                    Tick.OIDayLow = _getNum<int32_t>(packet, 56, 59) / divisor;
                    Tick.timestamp = _getNum<int32_t>(packet, 60, 63);

                    unsigned int depthStartIdx = 64;
                    for (int i = 0; i <= packetSize; i++) {

                        kitepp::depthWS depth;
                        depth.quantity = _getNum<int32_t>(packet, depthStartIdx, depthStartIdx + 3);
                        depth.price = _getNum<int32_t>(packet, depthStartIdx + 4, depthStartIdx + 7) / divisor;
                        depth.orders = _getNum<int32_t>(packet, depthStartIdx + 8, depthStartIdx + 9);

                        (i >= 5) ? Tick.marketDepth.sell.emplace_back(depth) : Tick.marketDepth.buy.emplace_back(depth);
                        depthStartIdx = depthStartIdx + 12;
                    };
                };
            };

            ticks.emplace_back(Tick);
        };

        return ticks;
    };

    void _pingLoop() {

        while (!_stop) {

            std::cout << "Sending ping..\n";
            if (_WS) {
                _WS->ping(_pingMessage.data());
                std::this_thread::sleep_for(std::chrono::seconds(_pingInterval));
            };
        };
    };
};

} // namespace kitepp