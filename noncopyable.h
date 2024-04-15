#pragma once

/*
    派生类对象可以正常析构和构造，但是派生类对象无法拷贝构造和赋值操作
*/

class noncopyable{
    public:
    noncopyable(const noncopyable&) = delete;
    void operator=(const noncopyable&) = delete;
    protected:
    noncopyable()  = default;
    ~noncopyable() = default;
};