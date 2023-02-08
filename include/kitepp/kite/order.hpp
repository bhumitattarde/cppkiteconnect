/*
 *  Licensed under the MIT License <http://opensource.org/licenses/MIT>.
 *  SPDX-License-Identifier: MIT
 *
 *  Copyright (c) 2020-2022 Bhumit Attarde
 *
 *  Permission is hereby  granted, free of charge, to any  person obtaining a
 * copy of this software and associated  documentation files (the "Software"),
 * to deal in the Software  without restriction, including without  limitation
 * the rights to  use, copy,  modify, merge,  publish, distribute,  sublicense,
 * and/or  sell copies  of  the Software,  and  to  permit persons  to  whom the
 * Software  is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS
 * OR IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN
 * NO EVENT  SHALL THE AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY
 * CLAIM,  DAMAGES OR  OTHER LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#pragma clang diagnostic ignored "-Wundefined-inline"

#include <vector>

#include "../kite.hpp"
#include "../utils.hpp"

namespace kiteconnect {
inline string kite::placeOrder(const placeOrderParams& params) {
    // required parameters
    utils::http::Params bodyParams = {
        { "exchange", params.exchange },
        { "tradingsymbol", params.symbol },
        { "transaction_type", params.transactionType },
        { "quantity", std::to_string(params.quantity) },
        { "product", params.product },
        { "order_type", params.orderType },
    };
    // optional parameters
    utils::addParam(bodyParams, params.price, "price");
    utils::addParam(bodyParams, params.validity, "validity");
    utils::addParam(bodyParams, params.disclosedQuantity, "disclosed_quantity");
    utils::addParam(bodyParams, params.triggerPrice, "trigger_price");
    utils::addParam(bodyParams, params.squareOff, "squareoff");
    utils::addParam(bodyParams, params.stopLoss, "stoploss");
    utils::addParam(bodyParams, params.trailingStopLoss, "trailing_stoploss");
    utils::addParam(bodyParams, params.icebergLegs, "iceberg_legs");
    utils::addParam(bodyParams, params.icebergQuantity, "iceberg_quantity");
    utils::addParam(bodyParams, params.validityTtl, "validity_ttl");
    utils::addParam(bodyParams, params.tag, "tag");

    return callApi<string, utils::json::JsonObject, true>("order.place",
        bodyParams, { params.variety }, [](utils::json::JsonObject& data) {
            return utils::json::get<string>(data, "order_id");
        });
};

inline string kite::modifyOrder(const modifyOrderParams& params) {
    utils::http::Params bodyParams = {};
    // optional parameters
    utils::addParam(bodyParams, params.parentOrderId, "parent_order_id");
    utils::addParam(bodyParams, params.quantity, "quantity");
    utils::addParam(bodyParams, params.price, "price");
    utils::addParam(bodyParams, params.orderType, "order_type");
    utils::addParam(bodyParams, params.triggerPrice, "trigger_price");
    utils::addParam(bodyParams, params.validity, "validity");
    utils::addParam(bodyParams, params.disclosedQuantity, "disclosed_quantity");

    return callApi<string, utils::json::JsonObject, true>("order.modify",
        bodyParams, { params.variety, params.orderId },
        [](utils::json::JsonObject& data) {
            return utils::json::get<string>(data, "order_id");
        });
};

inline string kite::cancelOrder(
    const string& variety, const string& orderId, const string& parentOrderID) {
    string endpoint;
    utils::FmtArgs fmtArgs;
    if (variety == "bo") {
        endpoint = "order.cancel.bo";
        fmtArgs = { variety, orderId, parentOrderID };
    } else {
        endpoint = "order.cancel";
        fmtArgs = { variety, orderId };
    };
    return callApi<string, utils::json::JsonObject, true>(
        endpoint, {}, fmtArgs, [](utils::json::JsonObject& data) {
            return utils::json::get<string>(data, "order_id");
        });
};

inline std::vector<order> kite::orders() {
    return callApi<std::vector<order>, utils::json::JsonArray, true>(
        "orders", {}, {}, [](utils::json::JsonArray& data) {
            std::vector<order> Orders;
            for (auto& i : data) { Orders.emplace_back(i.GetObject()); }
            return Orders;
        });
};

inline std::vector<order> kite::orderHistory(const string& orderId) {
    return callApi<std::vector<order>, utils::json::JsonArray, true>(
        "order.info", {}, { orderId }, [](utils::json::JsonArray& data) {
            std::vector<order> history;
            for (auto& i : data) { history.emplace_back(i.GetObject()); }
            return history;
        });
};

inline std::vector<trade> kite::trades() {
    return callApi<std::vector<trade>, utils::json::JsonArray, true>(
        "trades", {}, {}, [](utils::json::JsonArray& data) {
            std::vector<trade> trades;
            for (auto& i : data) { trades.emplace_back(i.GetObject()); }
            return trades;
        });
};

inline std::vector<trade> kite::orderTrades(const string& orderId) {
    return callApi<std::vector<trade>, utils::json::JsonArray, true>(
        "order.trades", {}, { orderId }, [](utils::json::JsonArray& data) {
            std::vector<trade> trades;
            for (auto& i : data) { trades.emplace_back(i.GetObject()); }
            return trades;
        });
};

inline std::vector<orderMargins> kite::getOrderMargins(
    const std::vector<orderMarginsParams>& params) {
    utils::json::json<utils::json::JsonArray> ordersJson;
    ordersJson.array<orderMarginsParams>(
        params, [&](const orderMarginsParams& param, rj::Value& buffer) {
            ordersJson.field("exchange", param.exchange, &buffer);
            ordersJson.field("tradingsymbol", param.tradingsymbol, &buffer);
            ordersJson.field(
                "transaction_type", param.transactionType, &buffer);
            ordersJson.field("variety", param.variety, &buffer);
            ordersJson.field("product", param.product, &buffer);
            ordersJson.field("order_type", param.orderType, &buffer);
            ordersJson.field("quantity", param.quantity, &buffer);
            ordersJson.field("price", param.price, &buffer);
            ordersJson.field("trigger_price", param.triggerPrice, &buffer);
        });

    return callApi<std::vector<orderMargins>, utils::json::JsonArray, true>(
        "order.margins", { { "", ordersJson.serialize() } }, {},
        [](utils::json::JsonArray& data) {
            std::vector<orderMargins> margins;
            for (auto& i : data) { margins.emplace_back(i.GetObject()); }
            return margins;
        });
};
} // namespace kiteconnect