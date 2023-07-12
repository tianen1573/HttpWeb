#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <stack>
#include <utility>
#include <limits>
#include <float.h>

class Cal
{
private:
    std::string _cal_request;
    double _cal_reslut;
    std::vector<std::pair<std::string, int>> _word;
    int idx;    // 单词的下标
    int sym;    // 单词的性质
    int err;    // 语法分析错误码
    bool _zero; // 除零错误
    bool _bomb; // 数值爆炸 -- 也可以添加大数运算功能
private:
    // -----------------------词法分析-----------------------
    bool word_analysis(std::vector<std::pair<std::string, int>> &word, const std::string expr)
    {
        for (int i = 0; i < expr.length(); ++i)
        {
            // 如果是 + - * / ( )
            if (expr[i] == '(' || expr[i] == ')' || expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/')
            {
                std::string tmp;
                tmp.push_back(expr[i]);
                switch (expr[i])
                {
                case '+':
                    word.push_back(std::make_pair(tmp, 1));
                    break;
                case '-':
                    word.push_back(make_pair(tmp, 2));
                    break;
                case '*':
                    word.push_back(make_pair(tmp, 3));
                    break;
                case '/':
                    word.push_back(make_pair(tmp, 4));
                    break;
                case '(':
                    word.push_back(make_pair(tmp, 6));
                    break;
                case ')':
                    word.push_back(make_pair(tmp, 7));
                    break;
                }
            }
            // 如果是数字开头
            else if (expr[i] >= '0' && expr[i] <= '9')
            {
                std::string tmp;
                while (expr[i] >= '0' && expr[i] <= '9')
                {
                    tmp.push_back(expr[i]);
                    ++i;
                }
                if (expr[i] == '.')
                {
                    ++i;
                    if (expr[i] >= '0' && expr[i] <= '9')
                    {
                        tmp.push_back('.');
                        while (expr[i] >= '0' && expr[i] <= '9')
                        {
                            tmp.push_back(expr[i]);
                            ++i;
                        }
                    }
                    else
                    {
                        return -1; // .后面不是数字，词法错误
                    }
                }
                word.push_back(make_pair(tmp, 5));
                --i;
            }
            // 如果以.开头
            else
            {
                return true; // 以.开头，词法错误
            }
        }
        return false;
    }

    // -----------------------语法分析-----------------------
    // 读下一单词的种别编码
    void Next()
    {
        if (idx < _word.size())
            sym = _word[idx++].second;
        else
            sym = 0;
    }
    // F → (E) | d
    void F()
    {
        if (sym == 5)
        {
            Next();
        }
        else if (sym == 6)
        {
            Next();
            E();
            if (sym == 7)
            {
                Next();
            }
            else
            {
                err = -1;
            }
        }
        else
        {
            err = -1;
        }
    }
    // T → F { *F | /F }
    void T()
    {
        F();
        while (sym == 3 || sym == 4)
        {
            Next();
            F();
        }
    }
    // E → T { +T | -T }
    void E()
    {
        T();
        while (sym == 1 || sym == 2)
        {
            Next();
            T();
        }
    }

    // -----------------------计算表达式-----------------------
    // 获取运算符的优先级
    int prior(int sym)
    {
        switch (sym)
        {
        case 1:
        case 2:
            return 1;
        case 3:
        case 4:
            return 2;
        default:
            return 0;
        }
    }
    // 通过 种别编码 判定是否是运算符
    bool isOperator(int sym)
    {
        switch (sym)
        {
        case 1:
        case 2:
        case 3:
        case 4:
            return true;
        default:
            return false;
        }
    }
    // 中缀转后缀
    std::vector<std::pair<std::string, int>> getPostfix(const std::vector<std::pair<std::string, int>> &expr)
    {
        std::vector<std::pair<std::string, int>> output; // 输出
        std::stack<std::pair<std::string, int>> stk;     // 操作符栈
        for (int i = 0; i < expr.size(); ++i)
        {
            std::pair<std::string, int> p = expr[i];
            if (isOperator(p.second))
            {
                while (!stk.empty() && isOperator(stk.top().second) && prior(stk.top().second) >= prior(p.second))
                {
                    output.push_back(stk.top());
                    stk.pop();
                }
                stk.push(p);
            }
            else if (p.second == 6)
            {
                stk.push(p);
            }
            else if (p.second == 7)
            {
                while (stk.top().second != 6)
                {
                    output.push_back(stk.top());
                    stk.pop();
                }
                stk.pop();
            }
            else
            {
                output.push_back(p);
            }
        }
        while (!stk.empty())
        {
            output.push_back(stk.top());
            stk.pop();
        }
        return output;
    }
    // 从栈中连续弹出两个操作数
    void popTwoNumbers(std::stack<double> &stk, double &first, double &second)
    {
        first = stk.top();
        stk.pop();
        second = stk.top();
        stk.pop();
    }
    // 把string转换为double
    double stringToDouble(const std::string &str)
    {
        double d;
        std::stringstream ss;
        ss << str;
        ss >> d;
        return d;
    }
    // 计算后缀表达式的值
    double expCalculate(const std::vector<std::pair<std::string, int>> &postfix)
    {
        double first, second;
        std::stack<double> stk;
        for (int i = 0; i < postfix.size(); ++i)
        {
            std::pair<std::string, int> p = postfix[i];
            switch (p.second)
            {
            case 1:
                popTwoNumbers(stk, first, second);
                if ((second + first) - second == first)
                {
                    stk.push(second + first);
                    break;
                }
                else
                {
                    _bomb = true;
                    stk.push(0); // 避免stk里无数据
                    goto END;
                }
            case 2:
                popTwoNumbers(stk, first, second);
                if ((second - first) + first == second)
                {
                    stk.push(second - first);
                    break;
                }
                else
                {
                    _bomb = true;
                    stk.push(0); // 避免stk里无数据
                    goto END;
                }
            case 3:
                popTwoNumbers(stk, first, second);
                if ((second * first) / first == second)
                {
                    stk.push(second * first);
                    break;
                }
                else
                {
                    _bomb = true;
                    stk.push(0); // 避免stk里无数据
                    goto END;
                }
            case 4:
                popTwoNumbers(stk, first, second);
                if (first == 0)
                {
                    _zero = true;
                    stk.push(0); // 避免stk里无数据
                    goto END;
                }
                if ((second / first) * first == second)
                {
                    stk.push(second / first);
                    break;
                }
                else
                {
                    _bomb = true;
                    stk.push(0); // 避免stk里无数据
                    goto END;
                }
            default:
                if (std::to_string(stringToDouble(p.first)) == p.first)
                {
                    stk.push(stringToDouble(p.first));
                    break;
                }
                else
                {
                    _bomb = true;
                    stk.push(0); // 避免stk里无数据
                    goto END;
                }
            }
        }
    END:
        double result = stk.top();
        stk.pop();
        return result;
    }

public:
    Cal(std::string &req)
        : _cal_request(req), idx(0), err(0), _zero(false), _bomb(false)
    {
    }

    bool LexicalAnalysis()
    {
        return word_analysis(_word, _cal_request);
    }
    bool Prase()
    {
        Next();
        E();

        return (err || sym);
    }
    void CalExpr()
    {
        _cal_reslut = expCalculate(getPostfix(_word));
    }

    bool IsBomb()
    {
        return _bomb;
    }
    bool IsZero()
    {
        return _zero;
    }
    double Reslut()
    {
        return _cal_reslut;
    }

    ~Cal()
    {
    }
};
