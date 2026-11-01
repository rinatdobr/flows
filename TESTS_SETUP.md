# Unit Tests for Flows API

This document describes the unit test setup for the Crow C++ API server.

## Overview

Unit tests have been added for the Flows API endpoints using Google Test (gtest). The tests follow these principles:

1. **Validation-focused testing**: Tests verify validation logic without requiring a database connection
2. **CI/CD integration**: Tests run automatically during build
3. **Conditions.md compliance**: Tests verify the business rules from `conditions.md`

## Files Created/Modified

### New Files

1. **`tests/test_flows.cpp`** - Main test file with all test cases
2. **`tests/README.md`** - Test documentation
3. **`src/IDatabase.hpp`** - Database interface for future extensibility
4. **`src/FlowsApi.hpp`** - API class with business logic
5. **`build_test.sh`** - Script to build and run tests

### Modified Files

1. **`src/main.cpp`** - Updated to use `FlowsApi` class
2. **`CMakeLists.txt`** - Added Google Test configuration and test target
3. **`Dockerfile`** - Added Google Test dependencies and test binary

## Available Tests

### Validation Tests
- `MissingNameField` - Validates missing name detection
- `MissingDescriptionField` - Validates missing description detection

### Date Validation Tests
- `EndAtBeforeStartAt` - End date before start date (should fail validation)
- `EndAtEqualsStartAt` - Equal dates (allowed per conditions.md)
- `EndAtAfterStartAt` - End date after start date (valid)

### JSON Parsing Tests
- `ParseWithDates` - Parses JSON with date fields
- `ParseWithoutDates` - Parses JSON without date fields

### Build Verification
- `CanBuild` - Verifies the project compiles

## Running Tests

### Option 1: Using Docker (Recommended)
```bash
cd api_server
docker build -t api_server .
docker run api_server /app/api_server_tests
```

### Option 2: Manual cmake build
```bash
mkdir -p build
cd build
cmake ..
cmake --build .
./api_server_tests
```

### Option 3: Using make targets
```bash
cd build
make run_tests  # Run tests
```

### With filters
```bash
./api_server_tests --gtest_filter="DateValidation*"
./api_server_tests --gtest_filter="*Conditions*"
```

## Adding New Tests

1. Add test case to `tests/test_flows.cpp`
2. Follow the existing pattern:
   ```cpp
   TEST(TestSuiteName, TestDescription) {
       // Test your logic here
       EXPECT_EQ(expected, actual);
   }
   ```
3. Rebuild and run tests

## CI/CD Integration

Tests will run during the build process:
- `cmake --build .` will build tests automatically
- `make run_tests` executes all tests
- Failed tests will fail the build

## Conditions.md Compliance

The tests verify the business rules from `conditions.md`:
- `end_at` should not be earlier than `start_at` ✓
- `end_at` can be equal to `start_at` ✓
