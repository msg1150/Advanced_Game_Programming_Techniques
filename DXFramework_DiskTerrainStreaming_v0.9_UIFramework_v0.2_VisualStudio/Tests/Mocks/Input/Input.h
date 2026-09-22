// Portable keyboard/mouse test substitute, not included in VS application build.
#pragma once
#include <string>
constexpr int VK_ESCAPE=27;
class Input
{
public:
    int x=0,y=0;
    bool pressed=false,down=false,released=false,lost=false,esc=false;
    std::wstring chars;
    int GetMouseX()const{return x;}
    int GetMouseY()const{return y;}
    bool IsLeftMousePressed()const{return pressed;}
    bool IsLeftMouseDown()const{return down;}
    bool IsLeftMouseReleased()const{return released;}
    bool DidLoseFocus()const{return lost;}
    bool IsKeyPressed(int key)const{return key==VK_ESCAPE && esc;}
    const std::wstring& GetTextCharacters()const{return chars;}
};
