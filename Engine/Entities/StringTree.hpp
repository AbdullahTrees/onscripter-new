/**
 *  StringTree.hpp
 *  ONScripter-RU
 *
 *  A hierarchical tree-like structure.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <unordered_map>
#include <string>
#include <deque>
#include <vector>
#include <tuple>

class StringTree {
public:
	struct IStringTreeVisitor {
		virtual void visit(StringTree &tree) = 0;
		virtual ~IStringTreeVisitor() = default;
	};

	struct StringTreeExecuter : public IStringTreeVisitor {
		void visit(StringTree &tree) override;
		bool realVisit(StringTree &tree);
	};

	struct StringTreePrinter : public IStringTreeVisitor {
		void visit(StringTree &tree) override;
		void realVisit(StringTree &tree, int indent);
	};

	// API is simply too restrictive with these private, you should have seen that hell
	std::string value;
	std::unordered_map<std::string, cmp::any<StringTree>> branches;
	std::vector<std::string> insertionOrder;

	void accept(const std::shared_ptr<IStringTreeVisitor> &visitor);
	std::string getValue(std::deque<std::string> &ss);
	int setValue(std::deque<std::string> &ss, std::string &value);
	void prune(std::deque<std::string> &ss);
	void clear();
	bool has(const std::string &key) {
		return branches.count(key);
	}
	bool has(long key) {
		return branches.count(std::to_string(key));
	}
	StringTree &operator[](std::string &&key) {
		auto it = branches.find(key);
		if (it == branches.end()) {
			auto inserted = branches.emplace(std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::forward_as_tuple());
			it = inserted.first;
			insertionOrder.push_back(it->first);
		}
		return it->second;
	}
	StringTree &operator[](const std::string &key) {
		auto it = branches.find(key);
		if (it == branches.end()) {
			auto inserted = branches.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple());
			it = inserted.first;
			insertionOrder.push_back(it->first);
		}
		return it->second;
	}
	StringTree &operator[](const long key) {
		return operator[](std::to_string(key));
	}
	StringTree &getById(long key) {
		return (*this)[insertionOrder[key]];
	}
};
