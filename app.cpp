#include <boost/asio.hpp>
#include <boost/asio/streambuf.hpp>
#include <string>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <vector>

using Json = nlohmann::json;

class Item {
    private:
        std::string name;
        double price;
    public:
        Item(std::string name, double price) : name(name), price(price) {}
        std::string getName() { return name; }
        double getPrice() { return price; }
    bool operator==(const Item &other) const {
        return name == other.name && price == other.price;
    }
};

class ShoppingCart {
    private:
        std::vector<Item> items;

    public:
        void addItem(Item item) {
            items.push_back(item);
        }

        void removeItem(Item item) {
            items.erase(std::remove(items.begin(), items.end(), item), items.end());
        }

        int getNumItems() {
            return items.size();
        }

        double getTotalCost() {
            double totalCost = 0;
            for (auto &item : items) {
                totalCost += item.getPrice();
            }
            return totalCost;
        }

        nlohmann::json serialize() {
            nlohmann::json json_items;
            for (auto &item : items) {
                json_items.push_back({{"name", item.getName()},{"price", item.getPrice()}});
            }
            nlohmann::json json_cart;
            json_cart["items"] = json_items;
            return json_cart;
        }

        void save() {
            std::ofstream file("cart.json");
            file << this->serialize().dump(4);
            file.close();
        }

        std::string getCartContentsAsHTMLTable() {
            std::string table = "<table style='border: 1px solid black; border-collapse: collapse; width: 100%;'>\n";
            table += "<tr style='border: 1px solid black;'>\n";
            table += "<th style='border: 1px solid black; padding: 5px;'>Name</th>\n";
            table += "<th style='border: 1px solid black; padding: 5px;'>Price</th>\n";
            table += "</tr>\n";
            for (auto &item : items) {
                table += "<tr style='border: 1px solid black;'>\n";
                table += "<td style='border: 1px solid black; padding: 5px;'>" + item.getName() + "</td>\n";
                table += "<td style='border: 1px solid black; padding: 5px;'>" + std::to_string(item.getPrice()) + "</td>\n";
                table += "</tr>\n";
            }
            table += "</table>";
            return table;
        }

        void readFromJSON(std::string jsonFile) {
            // Read file into string
            std::ifstream jsonStream(jsonFile);
            std::string jsonString((std::istreambuf_iterator<char>(jsonStream)),
                                std::istreambuf_iterator<char>());

            // Parse json string
            nlohmann::json json;
            try
            {
                json = nlohmann::json::parse(jsonString);
            }
            catch(nlohmann::json::parse_error& e)
            {
                // Error parsing json
                return;
            }

            // Iterate through the items array
            for (auto& item : json["items"]) {
                // Get name and price of item
                std::string name = item["name"];
                double price = item["price"];

                // Create item and add to cart
                Item item_to_add(name, price);
                this->addItem(item_to_add);
            }
        }
};

std::string read_request(boost::asio::ip::tcp::socket& socket) {
    std::string request_string;
    boost::asio::streambuf request_buf;
    boost::asio::read_until(socket, request_buf, "\r\n\r\n");
    request_string = boost::asio::buffer_cast<const char*>(request_buf.data());
    return request_string;
};

std::map<std::string, std::string> get_request_parameters( std::string request_string ) {
    std::string body = request_string.substr(request_string.find("\r\n\r\n") + 4)+"&";

    std::map<std::string, std::string> parameters;
    std::string delimiter = "&";
    size_t pos = 0;
    std::string token;
    while ((pos = body.find(delimiter)) != std::string::npos) {
        token = body.substr(0, pos);
        size_t equal_pos = token.find("=");
        std::string key = token.substr(0, equal_pos);
        std::string value = token.substr(equal_pos + 1);
        parameters[key] = value;
        body.erase(0, pos + delimiter.length());
    }

    return parameters;
}

std::string urldecode(std::string encoded_string) {
    std::string decoded_string;
    for (std::string::iterator it = encoded_string.begin(); it != encoded_string.end(); ++it) {
        if (*it == '%') {
            decoded_string += (char)std::stoi(std::string(it + 1, it + 3), nullptr, 16);
            it += 2;
        } else if (*it == '+') {
            decoded_string += ' ';
        } else {
            decoded_string += *it;
        }
    }
    return decoded_string;
}

int main() {
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 8085);
    boost::asio::ip::tcp::acceptor acceptor(ioc, endpoint);

    while (true) {
        boost::asio::ip::tcp::socket socket(ioc);
        acceptor.accept(socket);

        std::cout << "Client connected from: " << socket.remote_endpoint() << std::endl;

        // Create shopping cart
        ShoppingCart cart;
        cart.readFromJSON("cart.json");

        // Read request
        std::string request_string = read_request( socket );

        // Read parameters from request
        std::map<std::string, std::string> parameters = get_request_parameters( request_string );

        // Add cart item from request
        if( parameters.count("name") > 0 ) {
            double price = std::stod( parameters["price"] );
            std::string decoded_name = urldecode(parameters["name"]);
            cart.addItem(Item(decoded_name, price));
            cart.save();
        }

        std::string cart_table = cart.getCartContentsAsHTMLTable();

        // Create add item form
        std::string cart_form = "<form method=\"post\">"
                               "<input type=\"text\" name=\"name\" placeholder=\"Name\" /> "
                               "<input type=\"text\" name=\"price\" size=\"5\" placeholder=\"Price\" /> "
                               "<input type=\"submit\" value=\"Add to cart\" />"
                               "</form>";

        // Create HTML response
        std::string html = "<html>"
                           "<head>"
                           "<title>My C++ Website</title>"
                           "</head>"
                           "<body>"
                           "<h1>Welcome to my C++ Website!</h1>"
                           "<p>This website is generated by a C++ program.</p>"
                           ""+cart_form+""
                           "<h2>Cart</h2>"
                           ""+cart_table+""
                           "</body>"
                           "</html>";

        // Get content length
        int content_length = html.length();
        std::string content_length_string = std::to_string(content_length);

        // Add headers to response
        std::string headers = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "+content_length_string+"\r\nConnection: close\r\n\r\n";

        // Send headers
        boost::asio::write(socket, boost::asio::buffer(headers));

        // Send HTML
        boost::asio::write(socket, boost::asio::buffer(html));
    }

    return 0;
}
