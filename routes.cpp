#include "routes.h"
#include "database.h"

#include <pqxx/pqxx>
#include <openssl/sha.h>
#include <openssl/rand.h>

#include <sstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <vector>
#include <stdexcept>

// ============================================================
// PASSWORD HELPERS
// ============================================================

static std::string toHex(
    const unsigned char* data,
    size_t len)
{
    std::ostringstream oss;

    for (size_t i = 0; i < len; ++i)
    {
        oss << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(data[i]);
    }

    return oss.str();
}

static std::string generateSalt(size_t bytes = 16)
{
    unsigned char buf[16];

    if (bytes > sizeof(buf))
        bytes = sizeof(buf);

    if (RAND_bytes(buf, static_cast<int>(bytes)) != 1)
        throw std::runtime_error("Failed to generate random salt");

    return toHex(buf, bytes);
}

static std::string hashPassword(
    const std::string& password,
    const std::string& salt)
{
    std::string combined = salt + password;

    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256(
        reinterpret_cast<const unsigned char*>(
            combined.c_str()
        ),
        combined.size(),
        hash
    );

    return toHex(hash, SHA256_DIGEST_LENGTH);
}

static bool isValidEmail(const std::string& email)
{
    static const std::regex pattern(
        R"((\w+)(\.\w+)*@(\w+)(\.\w+)+)"
    );

    return std::regex_match(email, pattern);
}

// ============================================================
// JSON RESPONSE
// ============================================================

static crow::response jsonResponse(
    int code,
    crow::json::wvalue body)
{
    crow::response res(code, std::move(body));

    res.set_header(
        "Content-Type",
        "application/json"
    );

    res.set_header(
        "Access-Control-Allow-Origin",
        "*"
    );

    res.set_header(
        "Access-Control-Allow-Methods",
        "GET, POST, PUT, DELETE, OPTIONS"
    );

    res.set_header(
        "Access-Control-Allow-Headers",
        "Content-Type, Authorization"
    );

    return res;
}

static crow::response errorResponse(
    int code,
    const std::string& message)
{
    crow::json::wvalue body;

    body["success"] = false;
    body["message"] = message;

    return jsonResponse(
        code,
        std::move(body)
    );
}

// ============================================================
// PRODUCT JSON
// ============================================================

static crow::json::wvalue productToJson(
    const pqxx::row_ref& row)
{
    crow::json::wvalue p;

    p["id"] = row["id"].as<int>();

    p["name"] =
        row["name"].as<std::string>();

    p["description"] =
        row["description"].is_null()
        ? ""
        : row["description"].as<std::string>();

    p["price"] =
        row["price"].as<double>();

    p["category"] =
        row["category"].is_null()
        ? ""
        : row["category"].as<std::string>();

    p["image_url"] =
        row["image_url"].is_null()
        ? ""
        : row["image_url"].as<std::string>();

    p["stock"] =
        row["stock"].as<int>();

    return p;
}

// ============================================================
// REGISTER ALL ROUTES
// ============================================================

void registerRoutes(IoTMartApp& app)
{
    // ========================================================
    // HEALTH CHECK
    // ========================================================

    CROW_ROUTE(app, "/health")
    .methods(
        crow::HTTPMethod::Get,
        crow::HTTPMethod::Options
    )
    ([](const crow::request& req)
    {
        if (req.method == crow::HTTPMethod::Options)
        {
            return jsonResponse(
                204,
                crow::json::wvalue{}
            );
        }

        crow::json::wvalue body;

        body["status"] = "ok";
        body["message"] = "IoTMart API is running";

        return jsonResponse(
            200,
            std::move(body)
        );
    });


    // ========================================================
    // TEST
    // ========================================================

    CROW_ROUTE(app, "/api/test")
    .methods(
        crow::HTTPMethod::Get,
        crow::HTTPMethod::Options
    )
    ([](const crow::request& req)
    {
        if (req.method == crow::HTTPMethod::Options)
        {
            return jsonResponse(
                204,
                crow::json::wvalue{}
            );
        }

        crow::json::wvalue body;

        body["success"] = true;
        body["message"] =
            "IoTMart API is running";

        return jsonResponse(
            200,
            std::move(body)
        );
    });


    // ========================================================
    // REGISTER
    // ========================================================

    CROW_ROUTE(app, "/api/register")
    .methods(
        crow::HTTPMethod::Post,
        crow::HTTPMethod::Options
    )
    ([](const crow::request& req)
    {
        if (req.method == crow::HTTPMethod::Options)
        {
            return jsonResponse(
                204,
                crow::json::wvalue{}
            );
        }

        auto body =
            crow::json::load(req.body);

        if (!body)
        {
            return errorResponse(
                400,
                "Invalid JSON body"
            );
        }

        std::string name =
            body.has("name")
            ? std::string(body["name"].s())
            : "";

        std::string email =
            body.has("email")
            ? std::string(body["email"].s())
            : "";

        std::string password =
            body.has("password")
            ? std::string(body["password"].s())
            : "";

        if (
            name.empty() ||
            email.empty() ||
            password.empty()
        )
        {
            return errorResponse(
                400,
                "name, email and password are required"
            );
        }

        if (!isValidEmail(email))
        {
            return errorResponse(
                400,
                "Invalid email format"
            );
        }

        if (password.size() < 6)
        {
            return errorResponse(
                400,
                "Password must be at least 6 characters"
            );
        }

        try
        {
            auto conn =
                Database::connect();

            pqxx::work txn(*conn);

            pqxx::result existing =
                txn.exec_params(
                    "SELECT id "
                    "FROM users "
                    "WHERE email = $1",
                    email
                );

            if (!existing.empty())
            {
                return errorResponse(
                    409,
                    "Email already registered"
                );
            }

            std::string salt =
                generateSalt();

            std::string storedHash =
                salt + "$" +
                hashPassword(
                    password,
                    salt
                );

            pqxx::result result =
                txn.exec_params(
                    "INSERT INTO users "
                    "(name, email, password) "
                    "VALUES ($1, $2, $3) "
                    "RETURNING id",
                    name,
                    email,
                    storedHash
                );

            txn.commit();

            crow::json::wvalue response;

            response["success"] = true;
            response["message"] =
                "Registration successful";

            response["user"]["id"] =
                result[0]["id"].as<int>();

            response["user"]["name"] =
                name;

            response["user"]["email"] =
                email;

            return jsonResponse(
                201,
                std::move(response)
            );
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "Register error: "
                << e.what()
                << std::endl;

            return errorResponse(
                500,
                "Server error, please try again"
            );
        }
    });


    // ========================================================
    // LOGIN
    // ========================================================

    CROW_ROUTE(app, "/api/login")
    .methods(
        crow::HTTPMethod::Post,
        crow::HTTPMethod::Options
    )
    ([](const crow::request& req)
    {
        if (req.method == crow::HTTPMethod::Options)
        {
            return jsonResponse(
                204,
                crow::json::wvalue{}
            );
        }

        auto body =
            crow::json::load(req.body);

        if (!body)
        {
            return errorResponse(
                400,
                "Invalid JSON body"
            );
        }

        std::string email =
            body.has("email")
            ? std::string(body["email"].s())
            : "";

        std::string password =
            body.has("password")
            ? std::string(body["password"].s())
            : "";

        if (
            email.empty() ||
            password.empty()
        )
        {
            return errorResponse(
                400,
                "email and password are required"
            );
        }

        try
        {
            auto conn =
                Database::connect();

            pqxx::work txn(*conn);

            pqxx::result result =
                txn.exec_params(
                    "SELECT id, name, password "
                    "FROM users "
                    "WHERE email = $1",
                    email
                );

            if (result.empty())
            {
                return errorResponse(
                    401,
                    "Invalid email or password"
                );
            }

            int id =
                result[0]["id"].as<int>();

            std::string userName =
                result[0]["name"].as<std::string>();

            std::string stored =
                result[0]["password"].as<std::string>();

            size_t separator =
                stored.find('$');

            if (separator ==
                std::string::npos)
            {
                return errorResponse(
                    500,
                    "Corrupt user record"
                );
            }

            std::string salt =
                stored.substr(
                    0,
                    separator
                );

            std::string hash =
                stored.substr(
                    separator + 1
                );

            if (
                hashPassword(
                    password,
                    salt
                ) != hash
            )
            {
                return errorResponse(
                    401,
                    "Invalid email or password"
                );
            }

            crow::json::wvalue response;

            response["success"] = true;
            response["message"] =
                "Login successful";

            response["user"]["id"] =
                id;

            response["user"]["name"] =
                userName;

            response["user"]["email"] =
                email;

            return jsonResponse(
                200,
                std::move(response)
            );
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "Login error: "
                << e.what()
                << std::endl;

            return errorResponse(
                500,
                "Server error, please try again"
            );
        }
    });


    // ========================================================
    // GET PRODUCTS
    // ========================================================

    CROW_ROUTE(app, "/api/products")
    .methods(
        crow::HTTPMethod::Get,
        crow::HTTPMethod::Options
    )
    ([](const crow::request& req)
    {
        if (req.method == crow::HTTPMethod::Options)
        {
            return jsonResponse(
                204,
                crow::json::wvalue{}
            );
        }

        try
        {
            auto conn =
                Database::connect();

            pqxx::work txn(*conn);

            pqxx::result result =
                txn.exec(
                    "SELECT id, name, description, "
                    "price, category, image_url, stock "
                    "FROM products "
                    "ORDER BY id ASC"
                );

            crow::json::wvalue::list items;

            for (const auto& row : result)
            {
                items.push_back(
                    productToJson(row)
                );
            }

            crow::json::wvalue response;

            response["success"] = true;

            response["products"] =
                std::move(items);

            return jsonResponse(
                200,
                std::move(response)
            );
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "Products error: "
                << e.what()
                << std::endl;

            return errorResponse(
                500,
                "Server error, please try again"
            );
        }
    });


    // ========================================================
    // ORDERS
    //
    // POST = Place Order
    // GET  = View Orders
    // OPTIONS = CORS
    // ========================================================

    CROW_ROUTE(app, "/api/orders")
    .methods(
        crow::HTTPMethod::Post,
        crow::HTTPMethod::Get,
        crow::HTTPMethod::Options
    )
    ([](const crow::request& req)
    {
        // ====================================================
        // OPTIONS
        // ====================================================

        if (req.method ==
            crow::HTTPMethod::Options)
        {
            return jsonResponse(
                204,
                crow::json::wvalue{}
            );
        }


        // ====================================================
        // POST /api/orders
        // PLACE ORDER
        // ====================================================

        if (req.method ==
            crow::HTTPMethod::Post)
        {
            auto body =
                crow::json::load(req.body);

            if (!body)
            {
                return errorResponse(
                    400,
                    "Invalid JSON body"
                );
            }

            if (
                !body.has("customer_name") ||
                !body.has("phone") ||
                !body.has("address") ||
                !body.has("payment_method") ||
                !body.has("items")
            )
            {
                return errorResponse(
                    400,
                    "customer_name, phone, address, payment_method and items are required"
                );
            }

            std::string customerName =
                body["customer_name"].s();

            std::string phone =
                body["phone"].s();

            std::string address =
                body["address"].s();

            std::string paymentMethod =
                body["payment_method"].s();

            int userId =
                body.has("user_id")
                ? body["user_id"].i()
                : 0;

            auto itemsJson =
                body["items"];

            if (itemsJson.size() == 0)
            {
                return errorResponse(
                    400,
                    "Cart is empty"
                );
            }

            if (customerName.empty() ||
                phone.empty() ||
                address.empty() ||
                paymentMethod.empty())
            {
                return errorResponse(
                    400,
                    "All customer details are required"
                );
            }

            try
            {
                auto conn =
                    Database::connect();

                pqxx::work txn(*conn);

                double total = 0.0;

                struct OrderLine
                {
                    int productId;
                    int qty;
                    double price;
                };

                std::vector<OrderLine> lines;

                // --------------------------------------------
                // CHECK PRODUCTS AND STOCK
                // --------------------------------------------

                for (
                    size_t i = 0;
                    i < itemsJson.size();
                    ++i
                )
                {
                    auto item =
                        itemsJson[i];

                    if (
                        !item.has("product_id") ||
                        !item.has("quantity")
                    )
                    {
                        return errorResponse(
                            400,
                            "Invalid cart item"
                        );
                    }

                    int productId =
                        item["product_id"].i();

                    int qty =
                        item["quantity"].i();

                    if (qty <= 0)
                    {
                        return errorResponse(
                            400,
                            "Invalid quantity for a cart item"
                        );
                    }

                    pqxx::result product =
                        txn.exec_params(
                            "SELECT price, stock "
                            "FROM products "
                            "WHERE id = $1",
                            productId
                        );

                    if (product.empty())
                    {
                        return errorResponse(
                            400,
                            "Product " +
                            std::to_string(productId) +
                            " not found"
                        );
                    }

                    double price =
                        product[0]["price"]
                            .as<double>();

                    int stock =
                        product[0]["stock"]
                            .as<int>();

                    if (stock < qty)
                    {
                        return errorResponse(
                            400,
                            "Insufficient stock for product " +
                            std::to_string(productId)
                        );
                    }

                    total +=
                        price * qty;

                    lines.push_back(
                        {
                            productId,
                            qty,
                            price
                        }
                    );
                }

                // --------------------------------------------
                // CREATE ORDER
                // --------------------------------------------

                pqxx::result orderResult;

                if (userId > 0)
                {
                    orderResult =
                        txn.exec_params(
                            "INSERT INTO orders "
                            "(user_id, customer_name, phone, "
                            "address, payment_method, total, status) "
                            "VALUES "
                            "($1, $2, $3, $4, $5, $6, 'Pending') "
                            "RETURNING id",

                            userId,
                            customerName,
                            phone,
                            address,
                            paymentMethod,
                            total
                        );
                }
                else
                {
                    orderResult =
                        txn.exec_params(
                            "INSERT INTO orders "
                            "(customer_name, phone, address, "
                            "payment_method, total, status) "
                            "VALUES "
                            "($1, $2, $3, $4, $5, 'Pending') "
                            "RETURNING id",

                            customerName,
                            phone,
                            address,
                            paymentMethod,
                            total
                        );
                }

                int orderId =
                    orderResult[0]["id"]
                        .as<int>();

                // --------------------------------------------
                // INSERT ORDER ITEMS + UPDATE STOCK
                // --------------------------------------------

                for (const auto& line : lines)
                {
                    txn.exec_params(
                        "INSERT INTO order_items "
                        "(order_id, product_id, quantity, price) "
                        "VALUES ($1, $2, $3, $4)",

                        orderId,
                        line.productId,
                        line.qty,
                        line.price
                    );

                    txn.exec_params(
                        "UPDATE products "
                        "SET stock = stock - $1 "
                        "WHERE id = $2",

                        line.qty,
                        line.productId
                    );
                }

                txn.commit();

                crow::json::wvalue response;

                response["success"] = true;

                response["message"] =
                    "Order placed successfully";

                response["order"]["id"] =
                    orderId;

                response["order"]["total"] =
                    total;

                response["order"]["status"] =
                    "Pending";

                return jsonResponse(
                    201,
                    std::move(response)
                );
            }
            catch (const std::exception& e)
            {
                std::cerr
                    << "Order creation error: "
                    << e.what()
                    << std::endl;

                return errorResponse(
                    500,
                    std::string("Order failed: ") +
                    e.what()
                );
            }
        }


        // ====================================================
        // GET /api/orders
        // VIEW ORDERS
        // ====================================================

        if (req.method ==
            crow::HTTPMethod::Get)
        {
            try
            {
                auto conn =
                    Database::connect();

                pqxx::work txn(*conn);

                const char* userIdParam =
                    req.url_params.get("user_id");

                std::string sql =
                    "SELECT id, user_id, "
                    "customer_name, phone, address, "
                    "payment_method, total, status, "
                    "created_at "
                    "FROM orders";

                if (userIdParam)
                {
                    sql +=
                        " WHERE user_id = " +
                        txn.quote(
                            std::string(
                                userIdParam
                            )
                        );
                }

                sql +=
                    " ORDER BY created_at DESC";

                pqxx::result orders =
                    txn.exec(sql);

                crow::json::wvalue::list orderList;

                for (const auto& row : orders)
                {
                    crow::json::wvalue order;

                    int orderId =
                        row["id"].as<int>();

                    order["id"] =
                        orderId;

                    if (!row["user_id"].is_null())
                    {
                        order["user_id"] =
                            row["user_id"].as<int>();
                    }

                    order["customer_name"] =
                        row["customer_name"]
                            .as<std::string>();

                    order["phone"] =
                        row["phone"]
                            .as<std::string>();

                    order["address"] =
                        row["address"]
                            .as<std::string>();

                    order["payment_method"] =
                        row["payment_method"]
                            .as<std::string>();

                    order["total"] =
                        row["total"]
                            .as<double>();

                    order["status"] =
                        row["status"]
                            .as<std::string>();

                    order["created_at"] =
                        row["created_at"]
                            .as<std::string>();

                    // ----------------------------------------
                    // ORDER ITEMS
                    // ----------------------------------------

                    pqxx::result items =
                        txn.exec_params(
                            "SELECT "
                            "oi.product_id, "
                            "oi.quantity, "
                            "oi.price, "
                            "p.name "
                            "FROM order_items oi "
                            "JOIN products p "
                            "ON p.id = oi.product_id "
                            "WHERE oi.order_id = $1",

                            orderId
                        );

                    crow::json::wvalue::list itemList;

                    for (const auto& itemRow : items)
                    {
                        crow::json::wvalue itemJson;

                        itemJson["product_id"] =
                            itemRow["product_id"]
                                .as<int>();

                        itemJson["name"] =
                            itemRow["name"]
                                .as<std::string>();

                        itemJson["quantity"] =
                            itemRow["quantity"]
                                .as<int>();

                        itemJson["price"] =
                            itemRow["price"]
                                .as<double>();

                        itemList.push_back(
                            std::move(itemJson)
                        );
                    }

                    order["items"] =
                        std::move(itemList);

                    orderList.push_back(
                        std::move(order)
                    );
                }

                crow::json::wvalue response;

                response["success"] = true;

                response["orders"] =
                    std::move(orderList);

                return jsonResponse(
                    200,
                    std::move(response)
                );
            }
            catch (const std::exception& e)
            {
                std::cerr
                    << "Orders list error: "
                    << e.what()
                    << std::endl;

                return errorResponse(
                    500,
                    std::string(
                        "Orders fetch failed: "
                    ) + e.what()
                );
            }
        }


        // ====================================================
        // OTHER METHOD
        // ====================================================

        return errorResponse(
            405,
            "Method Not Allowed"
        );
    });
}