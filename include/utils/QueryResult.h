#pragma once
#include <string>
#include <optional>
#include <utility>
#include <vector>
#include <functional>
#include <variant>
#include "database/DatabaseManager.h"

enum class QueryStatus {
    SUCCESS,                    // 查询成功，有数据
    NOT_FOUND,                 // 查询成功，但没有找到数据
    CONNECTION_ERROR,          // 数据库连接错误
    INTERNAL_ERROR             // 其他数据库内部错误
};

template<typename T = void>
struct QueryResult {
    QueryStatus status;
    std::optional<T> data;
    std::string error_message;

    QueryResult(QueryStatus s, std::optional<T> d = std::nullopt, std::string err = "")
        : status(s), data(std::move(d)), error_message(std::move(err)) {}

    QueryResult(const QueryResult& other) = default;
    QueryResult(QueryResult&& other) noexcept = default;
    QueryResult& operator=(const QueryResult& other) = default;
    QueryResult& operator=(QueryResult&& other) noexcept = default;

    template<typename U>
    static QueryResult<T> Success(U&& value) {
        return {QueryStatus::SUCCESS, std::forward<U>(value), ""};
    }

    static QueryResult<T> NotFound() {
        return {QueryStatus::NOT_FOUND, std::nullopt, ""};
    }

    static QueryResult<T> NotFound(const std::string& error) {
        return {QueryStatus::NOT_FOUND, std::nullopt, error};
    }

    static QueryResult<T> ConnectionError(const std::string& error = "Database connection error") {
        return {QueryStatus::CONNECTION_ERROR, std::nullopt, error};
    }

    static QueryResult<T> InternalError(const std::string& error = "Database internal error") {
        return {QueryStatus::INTERNAL_ERROR, std::nullopt, error};
    }

    bool isSuccess() const noexcept { return status == QueryStatus::SUCCESS; }
    bool isNotFound() const noexcept { return status == QueryStatus::NOT_FOUND; }
    bool isError() const noexcept { return status == QueryStatus::CONNECTION_ERROR || status == QueryStatus::INTERNAL_ERROR; }
    bool isConnectionError() const noexcept { return status == QueryStatus::CONNECTION_ERROR; }
    bool isInternalError() const noexcept { return status == QueryStatus::INTERNAL_ERROR; }


    template<typename U>
    static QueryResult<U> convertFrom(const QueryResult<ExecuteResult>& result, 
                                    std::function<U(const std::vector<std::string>&)> converter) {
        if (result.isConnectionError()) return QueryResult<U>::ConnectionError(result.error_message);
        if (result.isInternalError()) return QueryResult<U>::InternalError(result.error_message);
        if (result.isNotFound()) return QueryResult<U>::NotFound(result.error_message);
        return QueryResult<U>::Success(converter(std::get<std::vector<std::string>>(*result.data)));
    }


    static QueryResult<void> convertFrom(const QueryResult<ExecuteResult>& result) {
        if (result.isConnectionError()) return QueryResult<void>::ConnectionError(result.error_message);
        if (result.isInternalError()) return QueryResult<void>::InternalError(result.error_message);
        if (result.isNotFound()) return QueryResult<void>::NotFound(result.error_message);
        return QueryResult<void>::Success();
    }
};

template<>
struct QueryResult<void> {
    QueryStatus status;
    std::string error_message;

    QueryResult(QueryStatus s, std::string err = "")
        : status(s), error_message(std::move(err)) {}

    QueryResult(const QueryResult& other) = default;
    QueryResult(QueryResult&& other) noexcept = default;
    QueryResult& operator=(const QueryResult& other) = default;
    QueryResult& operator=(QueryResult&& other) noexcept = default;

    static QueryResult<void> Success() {
        return {QueryStatus::SUCCESS, ""};
    }

    static QueryResult<void> NotFound() {
        return {QueryStatus::NOT_FOUND, ""};
    }

    static QueryResult<void> NotFound(const std::string& error) {
        return {QueryStatus::NOT_FOUND, error};
    }

    static QueryResult<void> ConnectionError(const std::string& error = "Database connection error") {
        return {QueryStatus::CONNECTION_ERROR, error};
    }

    static QueryResult<void> InternalError(const std::string& error = "Database internal error") {
        return {QueryStatus::INTERNAL_ERROR, error};
    }

    bool isSuccess() const noexcept { return status == QueryStatus::SUCCESS; }
    bool isNotFound() const noexcept { return status == QueryStatus::NOT_FOUND; }
    bool isError() const noexcept { return status == QueryStatus::CONNECTION_ERROR || status == QueryStatus::INTERNAL_ERROR; }
    bool isConnectionError() const noexcept { return status == QueryStatus::CONNECTION_ERROR; }
    bool isInternalError() const noexcept { return status == QueryStatus::INTERNAL_ERROR; }
}; 