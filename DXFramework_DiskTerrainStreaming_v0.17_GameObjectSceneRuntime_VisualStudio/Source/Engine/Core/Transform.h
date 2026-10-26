#pragma once
#include "Physics/Collision/Core/CollisionMath.h"
#include <stdexcept>

namespace Engine
{
using Vec3=Collision::Vec3;
// Column-vector affine matrix. The graphics adapter transposes its convention.
struct Matrix
{
    Vec3 X{1,0,0},Y{0,1,0},Z{0,0,1},T{};
    Vec3 Vector(const Vec3& v)const{return X*v.x+Y*v.y+Z*v.z;}
    Vec3 Point(const Vec3& v)const{return Vector(v)+T;}
    Matrix operator*(const Matrix& b)const{return {Vector(b.X),Vector(b.Y),Vector(b.Z),Point(b.T)};}
    Vec3 InversePoint(const Vec3& p)const
    {
        const float determinant=Collision::Dot(X,Collision::Cross(Y,Z));
        if(std::abs(determinant)<1.e-8f)throw std::runtime_error("Singular parent transform");
        const Vec3 d=p-T;
        return {Collision::Dot(d,Collision::Cross(Y,Z))/determinant,
                Collision::Dot(d,Collision::Cross(Z,X))/determinant,
                Collision::Dot(d,Collision::Cross(X,Y))/determinant};
    }
};
struct Transform
{
    Vec3 Position{},Rotation{},Scale{1,1,1};
    Matrix LocalMatrix()const
    {
        const auto basis=Collision::BuildBasis(Rotation);
        return {basis.X*Scale.x,basis.Y*Scale.y,basis.Z*Scale.z,Position};
    }
    bool Valid()const
    {
        const auto finite=[](Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
        return finite(Position)&&finite(Rotation)&&finite(Scale)&&Scale.x>1.e-4f&&Scale.y>1.e-4f&&Scale.z>1.e-4f;
    }
};
}
