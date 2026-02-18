# Order Book Engine

C++17로 구현한 고성능 호가창(Limit Order Book) 엔진. 가격-시간 우선순위(Price-Time Priority) 기반의 주문 매칭을 지원한다.

## 주요 기능

- **지정가 주문(Limit Order)**: 지정 가격에 매수/매도 주문을 등록하고, 미체결 수량은 호가창에 대기
- **시장가 주문(Market Order)**: 최우선 호가부터 즉시 체결, 미체결 잔량은 폐기
- **주문 취소(Cancel)**: 해시맵을 통한 O(1) 조회 후 해당 가격 레벨 큐에서 제거
- **가격-시간 우선순위**: 최우선 가격부터 매칭하고, 동일 가격 내에서는 먼저 접수된 주문이 우선 체결

## 자료구조

| 자료구조 | 용도 |
|----------|------|
| `std::map<Price, std::deque<Order>, std::greater<>>` | 매수 호가 (높은 가격 우선) |
| `std::map<Price, std::deque<Order>>` | 매도 호가 (낮은 가격 우선) |
| `std::unordered_map<OrderId, OrderLocation>` | 주문 취소를 위한 O(1) 조회 |

## 빌드

CMake 3.14 이상, C++17 지원 컴파일러(MSVC, GCC, Clang) 필요.

```bash
cmake -B build
cmake --build build --config Release
```

## 실행

```bash
# 데모
./build/Release/orderbook

# 단위 테스트 (Google Test)
./build/Release/tests

# 벤치마크
./build/Release/benchmark
```

## 테스트

16개 단위 테스트:
- 지정가 주문 등록 및 호가창 상태 확인
- 가격-시간 우선순위 매칭 검증
- 시장가 매수/매도 체결 검증
- 빈 호가창에 시장가 주문 시 동작
- 주문 취소 (성공, 실패, 빈 가격 레벨 제거)
- 부분 체결(Partial Fill)
- 다수 가격 레벨에 걸친 매칭
- 호가 깊이(Depth) 제한 출력
- 유동성 초과 시장가 주문

## 벤치마크 결과

측정 환경: Windows 11, MSVC 19.44, Release 빌드 (500,000건)

| 연산 | 평균 (ns) | 중앙값 (ns) | P99 (ns) | 최소 (ns) | 최대 (ns) |
|------|-----------|-------------|----------|-----------|-----------|
| 지정가 주문 등록 | 989 | 700 | 2,500 | 100 | 34,758,500 |
| 주문 취소 | 1,284 | 1,000 | 5,500 | 0 | 5,213,800 |
| 시장가 주문 (매칭 포함) | 427 | 200 | 1,500 | 0 | 2,374,900 |

**처리량(Throughput)**: ~1,159,184 주문/초

## 프로젝트 구조

```
src/
  Types.h          - 공통 타입 정의 (Price, Quantity, OrderId, Side, OrderType 등)
  Order.h          - Order 구조체
  OrderBook.h      - OrderBook 클래스 선언
  OrderBook.cpp    - OrderBook 구현 (주문 등록, 취소, 매칭)
  main.cpp         - 데모 실행
bench/
  Benchmark.cpp    - 지연시간(Latency) 및 처리량(Throughput) 측정
tests/
  TestOrderBook.cpp - Google Test 단위 테스트
```
