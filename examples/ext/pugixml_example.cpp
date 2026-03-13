#include "ext/pugixml/pugixml.hpp"

#include <iostream>
#include <sstream>
#include <string>

int main()
{
    const char* xml = R"(
<?xml version="1.0" encoding="UTF-8"?>
<config>
    <server host="127.0.0.1" port="9000" />
    <database>
        <user>root</user>
        <password>123456</password>
    </database>
</config>
)";

    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_string(xml);
    if (!result)
    {
        std::cerr << "parse failed: " << result.description() << '\n';
        return 1;
    }

    pugi::xml_node server = doc.child("config").child("server");
    pugi::xml_node database = doc.child("config").child("database");

    if (!server || !database)
    {
        std::cerr << "required nodes not found\n";
        return 2;
    }

    std::cout << "server.host = " << server.attribute("host").as_string() << '\n';
    std::cout << "server.port = " << server.attribute("port").as_int() << '\n';
    std::cout << "database.user = " << database.child("user").text().as_string() << '\n';
    std::cout << "database.password = " << database.child("password").text().as_string() << '\n';

    server.attribute("port").set_value(9100);
    database.child("password").text().set("abcdef");

    pugi::xml_node log = doc.child("config").append_child("log");
    log.append_attribute("level") = "info";
    log.append_child(pugi::node_pcdata).set_value("enabled");

    std::ostringstream oss;
    doc.save(oss, "    ");

    std::cout << "\nserialized xml:\n";
    std::cout << oss.str() << '\n';

    const bool ok = doc.save_file("pugixml_demo_output.xml", "    ");
    std::cout << "\nsave_file = " << (ok ? "true" : "false") << '\n';

    return ok ? 0 : 3;
}