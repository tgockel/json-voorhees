#include <Arduino.h>

#include <jsonv/all.hpp>

void setup()
{
    Serial.begin(115200);

    jsonv::value parsed = jsonv::parse(R"({"answer":42})");
    Serial.print("answer=");
    Serial.println(static_cast<long>(parsed.at("answer").as_integer()));
    Serial.println(jsonv::to_string(parsed).c_str());
}

void loop()
{
}
