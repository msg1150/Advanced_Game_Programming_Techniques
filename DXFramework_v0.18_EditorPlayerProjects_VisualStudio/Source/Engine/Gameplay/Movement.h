#pragma once
#include "Engine/Core/Scene.h"
namespace Engine
{
using MoveFunction=std::function<void(GameObject&,Vec3)>;
class Movement final:public Component
{
public:
    explicit Movement(MoveFunction move={}):move_(std::move(move)){}
    float Speed=6.f;Vec3 InputDirection{};
    std::string Type()const override{return "Movement";}
    Properties Save()const override{return {{"speed",Text(Speed)}};}
    void Load(const Properties& p)override
    {Speed=Number(p,"speed",6);if(Speed<0||Speed>1000)throw std::runtime_error("Invalid movement speed");}
protected:
    void OnFixedUpdate(float dt)override
    {
        const float length=Collision::Length(InputDirection);Vec3 delta=(length>1?InputDirection/length:InputDirection)*(Speed*dt);
        if(move_)move_(Owner(),delta);else Owner().SetWorldPosition(Owner().WorldMatrix().T+delta);
    }
private:
    MoveFunction move_;
};
inline void RegisterGameplayComponents(ComponentRegistry& registry,MoveFunction move={})
{registry.Register("Movement",[move]{return std::make_unique<Movement>(move);});}
}
