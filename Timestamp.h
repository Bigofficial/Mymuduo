#pragma once


#include <iostream>
#include <string>

class Timestamp{
    public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch_);//必须显示指明，这里的话就不能隐式的将int64_t转化为Timestamp对象

    static Timestamp now();
    std::string toString() const; //表明不会对成员函数修改
    private:
    int64_t microSecondsSinceEpoch_;
};