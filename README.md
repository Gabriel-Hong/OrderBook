# Order Book Engine

C++20으로 구현한 고성능 호가창(Limit Order Book) 엔진. 가격-시간 우선순위(Price-Time Priority) 기반의 주문 매칭을 지원한다.

## 주요 기능

- **지정가 주문(Limit Order)**: 지정 가격에 매수/매도 주문을 등록하고, 미체결 수량은 호가창에 대기
- **시장가 주문(Market Order)**: 최우선 호가부터 즉시 체결, 미체결 잔량은 폐기
- **주문 취소(Cancel)**: O(1) 조회 및 O(1) 제거 (intrusive linked list)
- **가격-시간 우선순위**: 최우선 가격부터 매칭하고, 동일 가격 내에서는 먼저 접수된 주문이 우선 체결

## 자료구조

| 자료구조 | 용도 | 시간 복잡도 |
|----------|------|-------------|
| Flat array (`std::vector<PriceLevelList>`) | 매수/매도 호가 가격 레벨 | O(1) 가격 접근 |
| Intrusive doubly-linked list | 동일 가격 내 주문 큐 (FIFO) | O(1) 삽입/제거 |
| Object Pool (`OrderPool`) | Order 객체 사전 할당 | O(1) 할당/반환, 힙 할당 제거 |
| Flat vector (`std::vector<Order*>`) | OrderId → Order 포인터 직접 인덱싱 | O(1) 조회 |

## 빌드

CMake 3.14 이상, C++20 지원 컴파일러(MSVC, GCC, Clang) 필요.

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

측정 환경: Windows 11, MSVC 19.44, Release 빌드, LTO 활성화 (500,000건)

### 성능 개선 Before / After

| 연산 | Before (중앙값) | After (중앙값) | 개선율 |
|------|-----------------|----------------|--------|
| 지정가 주문 등록 | 500 ns | **200 ns** | **2.5x** |
| 주문 취소 | 1,000 ns | **200 ns** | **5.0x** |
| 시장가 주문 (매칭 포함) | 200 ns | **100 ns** | **2.0x** |
| 처리량 | 1.38M orders/sec | **4.87M orders/sec** | **3.5x** |

### After 상세 (최적화 후)

| 연산 | 평균 (ns) | 중앙값 (ns) | P99 (ns) | 최소 (ns) | 최대 (ns) |
|------|-----------|-------------|----------|-----------|-----------|
| 지정가 주문 등록 | 201 | 200 | 500 | 100 | 150,900 |
| 주문 취소 | 232 | 200 | 700 | 0 | 212,500 |
| 시장가 주문 (매칭 포함) | 225 | 100 | 900 | 100 | 61,700 |

### 적용한 최적화 기법

| # | 기법 | 변경 내용 | 효과 |
|---|------|-----------|------|
| 1 | `fills.reserve(16)` | 매칭 결과 벡터 사전 할당 | 매칭 hot path에서 동적 할당 제거 |
| 2 | Intrusive Linked List + Object Pool | `std::deque` → prev/next 내장 연결 리스트, 힙 할당 → 사전 할당 풀 | 취소 O(N) → O(1), 할당 ~100ns → ~1ns |
| 3 | Flat Array 가격 레벨 | `std::map` → `std::vector` 직접 인덱싱 | 가격 접근 O(log N) → O(1), 캐시 미스 제거 |
| 4 | Flat Vector 주문 조회 | `std::unordered_map` → `std::vector<Order*>` | 해시 계산 제거, O(1) 직접 인덱싱 |
| 5 | 벤치마크 Warm-up + CPU Pinning | 측정 전 캐시 안정화, `SetThreadAffinityMask` | Max 값 185x 감소 (27.9ms → 150μs) |
| 6 | LTO (Link-Time Optimization) | `CMAKE_INTERPROCEDURAL_OPTIMIZATION` | 파일 경계 인라이닝, 전반적 5~15% 개선 |
| 7 | `[[likely]]` / `[[unlikely]]` | C++20 분기 예측 힌트 | 매칭 루프 분기 예측 개선 |

## 프로젝트 구조

```
src/
  Types.h          - 공통 타입 정의 (Price, Quantity, OrderId, Side, OrderType 등)
  Order.h          - Order 구조체 (intrusive list용 prev/next 포함)
  OrderBook.h      - OrderBook, OrderPool, PriceLevelList 클래스 선언
  OrderBook.cpp    - OrderBook 구현 (주문 등록, 취소, 매칭)
  main.cpp         - 데모 실행
bench/
  Benchmark.cpp    - 지연시간(Latency) 및 처리량(Throughput) 측정
tests/
  TestOrderBook.cpp - Google Test 단위 테스트 (16개)
```
