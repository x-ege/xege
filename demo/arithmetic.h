/*
* arithmetic.cpp
*
*  Created on: 2016-9-10
*      Author: Wang Yang
*        Mail: admin@wysaid.org
*        Blog: wysaid.org
*/
/*
* arithmetic.h
*
*  Created on: 2016-9-10
*      Author: Wang Yang
*        Mail: admin@wysaid.org
*        Blog: wysaid.org
*/

#ifndef _ARITHMETIC_H_
#define _ARITHMETIC_H_

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <list>
#include <string>

/// 忽略 -Wmultichar 警告
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmultichar"
#endif

class ArithmeticNode
{
public:
	virtual ~ArithmeticNode() {}
	
	enum NodeType
	{
		INVALID,
		OPERATOR,
		CONSTANT,
		VARIABLE_X,
		VARIABLE_Y,
	};
	
	enum OperatorType
	{
		OP_PLUS = '+',
		OP_MINUS = '-',
		OP_MULTIPLY = '*',
		OP_DIVID = '/',
		OP_POWER = '^',
		
		OP_EXP = ' exp',
		OP_ABS = ' abs',
		OP_SIGN = 'sign',
		OP_SQRT = 'sqrt',
		
		OP_SIN = ' sin',
		OP_COS = ' cos',
		OP_ARCSIN = 'asin',
		OP_ARCCOS = 'acos',
		OP_TAN = ' tan',
		OP_ARCTAN = 'atan',
		OP_LOG = ' log',
		OP_LOG2 = 'log2',
		OP_LOG10 = '  lg',
		OP_INVALID = 0
	};
	
	virtual NodeType nodeType() const { return INVALID; }
	virtual OperatorType operatorType() const { return OP_INVALID; }
	virtual double value() = 0;
	virtual bool isValid() { return true; }
	
	virtual void setValue(NodeType type, double v) {}
	
	virtual void removeChildNodes() {}
	virtual const std::list<ArithmeticNode*>* childNode() const { return nullptr; }
	virtual std::list<ArithmeticNode*>* childNode() { return nullptr; }
	virtual void addChildNode(ArithmeticNode* node) {}
	
	virtual bool canReduce() { return false; } // 表达式是否能够简化, 不包含未知数的节点可以全部替换为 constant节点
	
	static ArithmeticNode::OperatorType getOpTypeByName(const char* name);
	static const char* getOpNameByType(ArithmeticNode::OperatorType opType);
};

template <typename ConstantType>
class ArithmeticNodeConstant : public ArithmeticNode
{
public:
	ArithmeticNodeConstant(ConstantType c) :
	m_constant(c) {}
	NodeType nodeType() const { return CONSTANT; }
	double value() { return (double)m_constant; } // A constant must be compitable with 'double'.
	inline const ConstantType constant() const { return m_constant; }
	bool canReduce() { return true; }
	
private:
	ConstantType m_constant;
};

template <int VariableType>
class ArithmeticNodeVariable : public ArithmeticNode
{
public:
	ArithmeticNodeVariable() :
	m_variable(0.0) { static_assert(VariableType == VARIABLE_X || VariableType == VARIABLE_Y, "Invalid Variable Type!"); };
	NodeType nodeType() const override { return (NodeType)VariableType; }
	double value() override { return m_variable; }
	void setValue(NodeType type, double v) override
	{
		if (type == VariableType)
			m_variable = v;
	}
	
	bool canReduce() override { return false; }
	
private:
	double m_variable;
};

class ArithmeticNodeOperatorInterface : public ArithmeticNode
{
public:
	virtual ~ArithmeticNodeOperatorInterface()
	{
		for (auto* node : m_childNode)
		{
			delete node;
		}
		m_childNode.clear();
	}
	
	void setValue(NodeType type, double v)
	{
		for (auto* node : m_childNode)
			node->setValue(type, v);
	}
	
	NodeType nodeType() const { return OPERATOR; }
	
	void removeChildNodes() { m_childNode.clear(); }
	const std::list<ArithmeticNode*>* childNode() const { return &m_childNode; }
	std::list<ArithmeticNode*>* childNode() { return &m_childNode; }
	void addChildNode(ArithmeticNode* node)
	{
		if (node != nullptr) m_childNode.push_back(node);
	}
	
	bool canReduce()
	{
		for (auto* node : m_childNode)
			if (!node->canReduce()) return false;
		return true;
	}
	
protected:
	std::list<ArithmeticNode*> m_childNode;
};

//////////////////////////////////////////////////////////////////////////

class ArithmeticNodeOperatorImp : public ArithmeticNodeOperatorInterface
{
public:
	ArithmeticNodeOperatorImp(OperatorType op) :
	m_op(op) {}
	
	bool isValid();
	double value();
	
	OperatorType operatorType() const { return m_op; }
	
private:
	OperatorType m_op;
};

////////////////////////////////////////////////////////////////////////

class ArithmeticExpression
{
public:
	ArithmeticExpression() :
	m_node(nullptr) {}
	explicit ArithmeticExpression(const std::string& equation);
	~ArithmeticExpression();
	
	void parse(const std::string& equation);
	bool canReduce();
	void reduceNode(); // Optimize the nodes.  简化表达式, 保留变量。 对于不包含变量的表达式， 将直接简化为一个 constant.
	double value();
	inline operator bool() { return isValid(); }
	inline bool isValid() { return m_node != nullptr && m_node->isValid(); }
	inline ArithmeticNode* node() { return m_node; }
	
	// Set variables. (设置未知数)
	void setX(double x);
	void setY(double y);
	
protected:
	void reduceNode(ArithmeticNode** pnode);
	
protected:
	ArithmeticNode* m_node;
};

#include <cmath>
#include <cstring>
#include <vector>

#ifndef M_E
#define M_E 2.71828182845904523536 // e
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846 // pi
#endif

#define LEFT_VAR m_childNode.front()->value()
#define RIGHT_VAR m_childNode.back()->value()
#define HELPER_2_ARG(op) \
return LEFT_VAR op RIGHT_VAR

#define BUFFER_SIZE 1024

constexpr std::string_view arithmeticOperators = "+-*/^";

using namespace std;

bool ArithmeticNodeOperatorImp::isValid()
{
	switch (m_op)
	{
	case OP_PLUS:
	case OP_MINUS:
	case OP_MULTIPLY:
	case OP_DIVID:
	case OP_POWER:
		if (m_childNode.size() == 2)
			return m_childNode.front()->isValid() && m_childNode.back()->isValid();
		break;
	default:
		if (m_childNode.size() == 1)
			return m_childNode.front()->isValid();
		break;
	}
	
	return false;
}

double ArithmeticNodeOperatorImp::value()
{
	if (!isValid())
		return 0.0;
	
	switch (m_op)
	{
	case OP_PLUS:
		HELPER_2_ARG(+);
	case OP_MINUS:
		HELPER_2_ARG(-);
	case OP_MULTIPLY:
		HELPER_2_ARG(*);
	case OP_DIVID:
		HELPER_2_ARG(/);
	case OP_POWER:
		return pow(LEFT_VAR, RIGHT_VAR);
		
	case OP_EXP:
		return exp(LEFT_VAR);
	case OP_ABS:
		return fabs(LEFT_VAR);
	case OP_SIGN:
		return (LEFT_VAR > 0.0) - (LEFT_VAR < 0.0);
	case OP_SQRT:
		return sqrt(LEFT_VAR);
	case OP_SIN:
		return sin(LEFT_VAR);
	case OP_ARCSIN:
		return asin(LEFT_VAR);
	case OP_COS:
		return cos(LEFT_VAR);
	case OP_ARCCOS:
		return acos(LEFT_VAR);
	case OP_TAN:
		return tan(LEFT_VAR);
	case OP_ARCTAN:
		return atan(LEFT_VAR);
	case OP_LOG:
		return log(LEFT_VAR);
#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than vs2015
	case OP_LOG2:
		return log(LEFT_VAR) / log(2);
#else
	case OP_LOG2:
		return log2(LEFT_VAR);
#endif
	case OP_LOG10:
		return log10(LEFT_VAR);
	default:
		return 0.0;
	}
}

static const char* s_functions[] = {
	"exp",
	"abs",
	"sign",
	"sqrt",
	"sin",
	"asin",
	"arcsin",
	"cos",
	"acos",
	"arccos",
	"tan",
	"atan",
	"arctan",
	"ln",
	"log",
	"log2",
	"lg",
	"log10",
};

static ArithmeticNode::OperatorType s_opType[] = {
	ArithmeticNode::OP_EXP,    // exp
	ArithmeticNode::OP_ABS,    // abs
	ArithmeticNode::OP_SIGN,   // sign
	ArithmeticNode::OP_SQRT,   // sqrt
	ArithmeticNode::OP_SIN,    // sin
	ArithmeticNode::OP_ARCSIN, // asin
	ArithmeticNode::OP_ARCSIN, // arcsin
	ArithmeticNode::OP_COS,    // cos
	ArithmeticNode::OP_ARCCOS, // acos
	ArithmeticNode::OP_ARCCOS, // arccos
	ArithmeticNode::OP_TAN,    // tan
	ArithmeticNode::OP_ARCTAN, // atan
	ArithmeticNode::OP_ARCTAN, // arctan
	ArithmeticNode::OP_LOG,    // ln
	ArithmeticNode::OP_LOG,    // log
	ArithmeticNode::OP_LOG2,   // log2
	ArithmeticNode::OP_LOG10,  // lg
	ArithmeticNode::OP_LOG10,  // log10
};

ArithmeticNode::OperatorType ArithmeticNode::getOpTypeByName(const char* name)
{
	static_assert(sizeof(s_functions) / sizeof(*s_functions) == sizeof(s_opType) / sizeof(*s_opType), "Invalid Rule Strings!");
	
	auto type = ArithmeticNode::OP_INVALID;
	
	for (int i = 0; i != sizeof(s_functions) / sizeof(*s_functions); ++i)
	{
		if (strcmp(name, s_functions[i]) == 0)
		{
			return s_opType[i];
		}
	}
	
	return type;
}

const char* ArithmeticNode::getOpNameByType(ArithmeticNode::OperatorType opType)
{
	static_assert(sizeof(s_functions) / sizeof(*s_functions) == sizeof(s_opType) / sizeof(*s_opType), "Invalid Rule Strings!");
	
	for (int i = 0; i != sizeof(s_opType) / sizeof(*s_opType); ++i)
	{
		if (opType == s_opType[i])
		{
			return s_functions[i];
		}
	}
	
	return nullptr;
}

class Parser
{
public:
	Parser() = default;
	
	ArithmeticNode* parseNode(const string& equation)
	{
		mTmpEquations.clear();
		mBuffer[0] = '\0';
		
//		puts(equation.c_str());
		
		string result = extractEquation(equation);
		
		int index = 0;
//		for (auto& s : mTmpEquations)
//		{
//			printf("@%d = %6s,\n", index++, s.c_str());
//		}
//		
//		printf("@%d = %6s,\n", index, result.c_str());
		
		return parse(result);
	}
	
protected:
	// 消除所有优先级
	string extractEquation(string equation)
	{
		// 最高优先级为括号
		while (1)
		{
			auto startIndex = equation.find_last_of('(');
			if (startIndex == string::npos)
				break;
			
			auto endIndex = equation.find(')', startIndex + 1);
			if (endIndex == string::npos)
			{
				puts("Parentheses do not match!");
				equation = "";
				break;
			}
			else if (endIndex + 1 < equation.size() && arithmeticOperators.find_first_of(equation[endIndex + 1]) == string::npos && equation[endIndex + 1] != ')')
			{
				puts("Invalid operator. Note: (1+1)2 is invalid. Please write: (1+1)*2");
				equation = "";
				break;
			}
			
			string eq = equation.substr(startIndex + 1, endIndex - startIndex - 1);
			eq = extractOP(eq);
			sprintf(mBuffer, "@%d", (int)mTmpEquations.size());
			mTmpEquations.push_back(eq);
			
			// Check if the left parenthesis is used by trigonometric function. (检查括号是否前置三角函数等)
			if (startIndex != 0 && isalpha(equation[startIndex - 1]))
			{
				int opIndex = (int)startIndex - 2;
				while (opIndex >= 0 && isalpha(equation[opIndex]))
					--opIndex;
				if (opIndex < 0 || !isalpha(equation[opIndex])) ++opIndex;
				
				eq = equation.substr(opIndex, startIndex - opIndex);
				sprintf(mBuffer, "%s(@%d)", eq.c_str(), (int)mTmpEquations.size() - 1);
				mTmpEquations.push_back(mBuffer);
				sprintf(mBuffer, "@%d", (int)mTmpEquations.size() - 1);
				
				equation = equation.substr(0, opIndex) + mBuffer + equation.substr(endIndex + 1, equation.size() - endIndex - 1);
			}
			else
			{
				equation = equation.substr(0, startIndex) + mBuffer + equation.substr(endIndex + 1, equation.size() - endIndex - 1);
			}
		}
		
		equation = extractOP(equation);
		
		return equation;
	}
	
	string extractOP(string equation)
	{
		// 乘方
		equation = parseOperator(equation, "^");
		// 乘除
		equation = parseOperator(equation, "*/");
		// 加减
		equation = parseOperator(equation, "+-");
		return equation;
	}
	
	// op必须为同一优先级.
	string parseOperator(string equation, string op)
	{
		while (1)
		{
			int opIndex = -1, opCount = 0, sz = (int)equation.size();
			
			for (int i = 0; i != sz; ++i)
			{
				if (opIndex < 0 && op.find_first_of(equation[i]) != string::npos)
				{
					opIndex = i;
				}
				
				if (arithmeticOperators.find_first_of(equation[i]) != string::npos)
					++opCount;
			}
			
			if (opIndex < 0 || opCount == 1)
				break;
			
			int before, after;
			
			for (before = (int)opIndex - 1; before >= 0; --before)
			{
				int c = equation[before];
				if (!(isdigit(c) || c == '.' || c == '@' || c == 'x' || c == 'y'))
					break;
			}
			
			for (after = (int)opIndex + 1; after < equation.size(); ++after)
			{
				int c = equation[after];
				if (!(isdigit(c) || c == '.' || c == '@' || c == 'x' || c == 'y'))
					break;
			}
			
			if (before == opIndex - 1 || after == opIndex + 1)
			{
				puts("Invalid Equation!");
				equation = "";
				break;
			}
			
			sprintf(mBuffer, "@%d", (int)mTmpEquations.size());
			mTmpEquations.push_back(equation.substr(before + 1, after - before - 1));
			
			string eq;
			if (before >= 0) eq += equation.substr(0, before + 1);
			eq += mBuffer;
			if (after < equation.size()) eq += equation.substr(after, equation.size() - after);
			
			equation = eq;
		}
		
		return equation;
	}
	
	ArithmeticNode* parseSimpleNode(const string& s)
	{
		ArithmeticNode* node = nullptr;
		
		if (s.size() == 1)
		{
			if (s[0] == 'x')
				return new ArithmeticNodeVariable<ArithmeticNode::VARIABLE_X>();
			
			if (s[0] == 'y')
				return new ArithmeticNodeVariable<ArithmeticNode::VARIABLE_Y>();
			
			if (s[0] == 'e')
			{
				return new ArithmeticNodeConstant<double>(M_E);
			}
		}
		
		if (s.compare("pi") == 0)
		{
			return new ArithmeticNodeConstant<double>(M_PI);
		}
		
		auto opIndex = s.find_first_of('(');
		
		if (opIndex != string::npos)
		{
			string opName = s.substr(0, opIndex);
			auto opType = ArithmeticNode::getOpTypeByName(opName.c_str());
			if (opType != ArithmeticNode::OP_INVALID)
			{
				string opChild = s.substr(opIndex + 1, s.size() - opIndex - 2);
				auto* childNode = parse(opChild);
				
				if (childNode == nullptr)
				{
					printf("Invalid node: %s\n", opChild.c_str());
					return nullptr;
				}
				
				auto* opNode = new ArithmeticNodeOperatorImp(opType);
				opNode->addChildNode(childNode);
				return opNode;
			}
		}
		
		if (s[0] == '@')
		{
			int index;
			if (sscanf(s.c_str() + 1, "%d", &index) == 1 && index < mTmpEquations.size())
				return parse(mTmpEquations[index]);
			return nullptr;
		}
		
		double value;
		if (sscanf(s.c_str(), "%lf", &value) == 1)
		{
			node = new ArithmeticNodeConstant<double>(value);
		}
		else
		{
			printf("Invalid node: %s\n", s.c_str());
		}
		
		return node;
	}
	
	// Recursive parse for the equation tree.
	ArithmeticNode* parse(const string& equation)
	{
		if (equation.empty())
			return nullptr; // Invalid case.
		
		char op;
		std::string left, right;
		auto index = equation.find_first_of(arithmeticOperators.data());
		if (index != string::npos)
		{
			op = equation[index];
			left = equation.substr(0, index);
			if (index + 1 < equation.size())
				right = equation.substr(index + 1);
			
			if (left.empty() || right.empty())
			{
				printf("Invalid node: %s\n", equation.c_str());
				return nullptr;
			}
			else
			{
				ArithmeticNode *leftNode, *rightNode;
				leftNode = parseSimpleNode(left);
				rightNode = parseSimpleNode(right);
				ArithmeticNodeOperatorImp* opNode = new ArithmeticNodeOperatorImp((ArithmeticNodeOperatorImp::OperatorType)op);
				opNode->addChildNode(leftNode);
				opNode->addChildNode(rightNode);
				return opNode;
			}
		}
		
		return parseSimpleNode(equation);
	}
	
protected:
	char mBuffer[BUFFER_SIZE]; // for temp usage.
	
	vector<string> mTmpEquations;
};

////////////////////////////////////////////////

ArithmeticExpression::ArithmeticExpression(const std::string& equation) :
m_node(nullptr)
{
	parse(equation);
}

ArithmeticExpression::~ArithmeticExpression()
{
	delete m_node;
}

void ArithmeticExpression::parse(const string& equation)
{
	string eq = equation;
	auto pos = eq.find('=');
	if (pos != string::npos)
	{
		string left = eq.substr(0, pos);
		string right = eq.substr(pos + 1);
		eq = "(";
		eq += left + ")" + "-(" + right + ")";
	}
	
	// trim
	for (auto it = eq.begin(); it != eq.end();)
	{
		*it = tolower(*it);
		if (*it == '[' || *it == '{')
			*it = '(';
		else if (*it == ']' || *it == '}')
			*it = ')';
		
		if (!isgraph(*it))
			it = eq.erase(it);
		else
			++it;
	}
	
	delete m_node;
	Parser parser;
	m_node = parser.parseNode(eq);
}

bool ArithmeticExpression::canReduce()
{
	return m_node != nullptr ? m_node->canReduce() : false;
}

void ArithmeticExpression::reduceNode()
{
	reduceNode(&m_node);
}

void ArithmeticExpression::reduceNode(ArithmeticNode** pnode)
{
	ArithmeticNode* node = *pnode;
	
	if (node->canReduce())
	{
		auto* newNode = new ArithmeticNodeConstant<double>(node->value());
		delete *pnode;
		*pnode = newNode;
		return;
	}
	
	auto childNodes = node->childNode();
	
	if (childNodes != nullptr && !childNodes->empty())
	{
		for (auto& childNode : *childNodes)
		{
			if (childNode->canReduce())
			{
				auto* newNode = new ArithmeticNodeConstant<double>(childNode->value());
				delete childNode;
				childNode = newNode;
			}
			else
			{
				auto* grandChildNodes = childNode->childNode();
				if (grandChildNodes != nullptr && !grandChildNodes->empty())
				{
					for (auto& grandChildNode : *grandChildNodes)
					{
						reduceNode(&grandChildNode);
					}
				}
			}
		}
	}
}

double ArithmeticExpression::value()
{
	return m_node != nullptr ? m_node->value() :
#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than vs2015
	sqrtf(-1.0f);
#else
	NAN;
#endif
}

void ArithmeticExpression::setX(double x)
{
	if (m_node != nullptr)
		m_node->setValue(ArithmeticNode::VARIABLE_X, x);
}

void ArithmeticExpression::setY(double y)
{
	if (m_node != nullptr)
		m_node->setValue(ArithmeticNode::VARIABLE_Y, y);
}


#endif // !_ARITHMETIC_H_
