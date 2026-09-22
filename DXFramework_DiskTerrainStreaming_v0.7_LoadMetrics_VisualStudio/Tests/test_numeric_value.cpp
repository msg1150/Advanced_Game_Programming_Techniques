#include "UI/UINumericValue.h"
#include <cassert>
#include <cmath>
#include <string>
#include <iostream>
int main()
{
    float value=-1.f;
    assert(UINumericValue::ParseClamped(L"140",28.f,160.f,value)&&value==140.f);
    assert(UINumericValue::ParseClamped(L"999",28.f,160.f,value)&&value==160.f);
    assert(UINumericValue::ParseClamped(L"20",28.f,160.f,value)&&value==28.f);
    assert(UINumericValue::ParseClamped(L"137",50.f,400.f,value)&&value==137.f);
    assert(UINumericValue::ParseClamped(L"-3.5",28.f,160.f,value)&&value==28.f);
    assert(UINumericValue::ParseClamped(L"+42.5",28.f,160.f,value)&&value==42.5f);
    assert(!UINumericValue::ParseClamped(L"",28.f,160.f,value));
    assert(!UINumericValue::ParseClamped(L"-",28.f,160.f,value));
    assert(!UINumericValue::ParseClamped(L".",28.f,160.f,value));
    assert(!UINumericValue::ParseClamped(L"1..3",28.f,160.f,value));
    assert(!UINumericValue::ParseClamped(L"NaN",28.f,160.f,value));
    assert(!UINumericValue::ParseClamped(L"8x",28.f,160.f,value));
    assert(!UINumericValue::ParseClamped(L"1 2",28.f,160.f,value));
    assert(!UINumericValue::ParseClamped(L"12345678901234567",28.f,160.f,value));
    assert(!UINumericValue::ParseClamped(L"25",160.f,28.f,value));
    std::cout<<"PASS numeric parse, clamp, invalid inputs, unsnapped direct input\n";
}
