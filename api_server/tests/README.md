# Tests for api_server

This directory contains Google Test unit tests for the Crow C++ API server.

## Running Tests

### During Build
Tests are built automatically when you run cmake build:

```bash
cd api_server
mkdir -p build
cd build
cmake ..
cmake --build .
make run_tests  # Run all tests
```

### Using Docker
```bash
# Build with tests
docker build -t api_server .

# Run tests
docker run api_server /app/api_server_tests
```

### Running Tests Directly

```bash
# Build the test executable
cmake --build . --target api_server_tests

# Run the tests
./api_server_tests

# Run with specific filter
./api_server_tests --gtest_filter="DateValidation*"
```

## Test Coverage

The tests cover:

1. **Validation Tests**
   - Missing name field detection
   - Missing description field detection

2. **Date Validation Tests**
   - End date before start date (invalid)
   - End date equal to start date (valid)
   - End date after start date (valid)

3. **JSON Parsing Tests**
   - Parsing JSON with date fields
   - Parsing JSON without date fields

4. **Build Verification**
   - Verifies the project compiles successfully

## Test Architecture

The tests are designed to be lightweight and run without requiring a database connection:
- **Validation tests** verify the Crow JSON parsing logic
- **Date validation tests** verify the date comparison logic from `conditions.md`
- **JSON parsing tests** verify the request body parsing

## Adding New Tests

Add new test cases to `test_flows.cpp` following the existing patterns:

```cpp
TEST(TestSuiteName, TestDescription) {
    // Test your validation logic here
    EXPECT_EQ(expected, actual);
}
```

## Conditions.md Compliance

The tests verify the business rule from `conditions.md`:
- `end_at` should not be earlier than `start_at` ✓
- `end_at` can be equal to `start_at` ✓
