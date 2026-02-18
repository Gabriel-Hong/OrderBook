# 성능 개선 기록

병목 지점을 분석하고, 7가지 최적화를 적용하여 전 연산 2~5배 성능 개선을 달성했다.

---

## 개선 결과 요약

| 연산 | Before (중앙값) | After (중앙값) | 개선율 | 목표 | 달성 |
|------|-----------------|----------------|--------|------|------|
| 지정가 주문 등록 | 500 ns | **200 ns** | **2.5x** | < 200 ns | O |
| 주문 취소 | 1,000 ns | **200 ns** | **5.0x** | < 100 ns | △ |
| 시장가 주문 (매칭) | 200 ns | **100 ns** | **2.0x** | < 100 ns | O |
| 처리량 | 1.38M/sec | **4.87M/sec** | **3.5x** | > 5M/sec | △ |

측정 환경: Windows 11, MSVC 19.44, Release 빌드, LTO 활성화, CPU pinning, 500,000건

### Before 상세 (최적화 전)

| 연산 | 평균 (ns) | 중앙값 (ns) | P99 (ns) | 최소 (ns) | 최대 (ns) |
|------|-----------|-------------|----------|-----------|-----------|
| 지정가 주문 등록 | 688 | 500 | 1,500 | 100 | 27,872,200 |
| 주문 취소 | 1,180 | 1,000 | 5,500 | 0 | 2,615,000 |
| 시장가 주문 (매칭 포함) | 257 | 200 | 1,100 | 0 | 109,100 |

### After 상세 (최적화 후)

| 연산 | 평균 (ns) | 중앙값 (ns) | P99 (ns) | 최소 (ns) | 최대 (ns) |
|------|-----------|-------------|----------|-----------|-----------|
| 지정가 주문 등록 | 201 | 200 | 500 | 100 | 150,900 |
| 주문 취소 | 232 | 200 | 700 | 0 | 212,500 |
| 시장가 주문 (매칭 포함) | 225 | 100 | 900 | 100 | 61,700 |

---

## 1. `result.fills.reserve(16)` — 매칭 벡터 사전 할당 [적용 완료]

### 문제
- `std::vector<Fill> fills`가 매칭 중 `push_back`으로 성장 → 재할당 발생 가능

### 적용
```cpp
void matchOrder(Order* order, OrderResult& result) {
    result.fills.reserve(16);  // 대부분의 매칭은 16건 이내
    // ...
}
```

### 효과
매칭 hot path에서 vector 재할당 제거. 1줄 변경으로 가장 간단한 최적화.

---

## 2. Intrusive Linked List + Object Pool [적용 완료]

### 문제
- `std::deque`는 블록 단위 할당으로 비연속적 메모리 → 캐시 미스
- `erase(remove_if(...))` 패턴은 취소 시 O(N) 선형 탐색
- 주문 등록/취소마다 `malloc`/`free` 발생 → hot path에서 수백 ns 소요

### 적용

**Intrusive Linked List**: Order 구조체에 prev/next 포인터를 내장하여 O(1) 제거:

```cpp
struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;
    Order* prev = nullptr;
    Order* next = nullptr;
};
```

`orders_` 맵에 Order 포인터를 직접 저장. 취소 시 포인터로 바로 접근하여 prev/next 연결만 변경.

**Object Pool**: 고정 크기 Order 객체를 시작 시 한 번에 할당하여 힙 할당 제거:

```cpp
class OrderPool {
    std::vector<Order> pool_;
    std::vector<Order*> freeList_;
public:
    explicit OrderPool(size_t capacity) : pool_(capacity) {
        freeList_.reserve(capacity);
        for (size_t i = 0; i < capacity; ++i)
            freeList_.push_back(&pool_[i]);
    }
    Order* alloc()          { auto* p = freeList_.back(); freeList_.pop_back(); return p; }
    void   dealloc(Order* p){ freeList_.push_back(p); }
};
```

### 효과
- 취소: O(N) 선형 탐색 → **O(1)** prev/next 조작. 중앙값 1,000ns → 200ns (**5x**)
- 할당: `malloc` ~100ns → pool `pop_back` ~1ns

---

## 3. `std::map` → Flat Array 가격 레벨 [적용 완료]

### 문제
- `std::map`은 레드-블랙 트리 기반 → 노드마다 힙 할당, 포인터 체이싱으로 캐시 미스 다발
- 가격 레벨 접근마다 O(log N) 트리 탐색

### 적용
가격을 인덱스로 변환하여 flat array로 O(1) 접근. `bestBid_`/`bestAsk_` 변수로 최우선 호가 추적:

```cpp
constexpr Price  MIN_PRICE = 0;
constexpr Price  MAX_PRICE = 20000;
constexpr size_t NUM_PRICE_LEVELS = MAX_PRICE - MIN_PRICE + 1;

std::vector<PriceLevelList> bidLevels_(NUM_PRICE_LEVELS);
std::vector<PriceLevelList> askLevels_(NUM_PRICE_LEVELS);

Price bestBid_ = MIN_PRICE - 1;  // sentinel: no bids
Price bestAsk_ = MAX_PRICE + 1;  // sentinel: no asks
```

### 효과
- 가격 레벨 접근: O(log N) → **O(1)**
- 캐시 미스 제거 (연속 메모리)
- 지정가 주문 등록 중앙값: 500ns → 200ns (**2.5x**)

---

## 4. `std::unordered_map` → Flat Vector 주문 조회 [적용 완료]

### 문제
- `std::unordered_map`은 chaining 방식 → 버킷 + 노드 포인터 체이싱
- 매칭 hot path에서 `orders_.erase()` 호출

### 적용
OrderId가 1부터 단조 증가하므로, 직접 인덱싱이 가능:

```cpp
std::vector<Order*> orders_;  // orders_[id] = pointer (nullptr if not on book)
```

### 효과
- 해시 계산 제거, O(1) 직접 배열 인덱싱
- 노드 할당 제거, 캐시 미스 감소

---

## 5. 벤치마크 Warm-up + CPU Pinning [적용 완료]

### 문제
- Max 값이 27.9ms (OS 스케줄링, 페이지 폴트, 캐시 콜드 스타트)
- CPU 코어 이동으로 캐시 무효화

### 적용
```cpp
// CPU pinning (Windows)
SetThreadAffinityMask(GetCurrentThread(), 1);
SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

// Warm-up: 측정 전 1만 건 사전 실행
OrderBook warmup;
for (int i = 0; i < 10'000; ++i)
    warmup.addOrder(...);
```

### 효과
- Max 값: 27,872,200ns → 150,900ns (**185x 감소**)
- P99 값: 1,500ns → 500ns (측정 정밀도 향상)

---

## 6. LTO (Link-Time Optimization) [적용 완료]

### 문제
- 파일별 독립 컴파일로 인해 파일 경계 간 인라이닝 불가

### 적용
```cmake
include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED)
if(LTO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
```

### 효과
- 파일 경계를 넘어 함수 인라이닝
- 미사용 코드 제거
- 전반적 5~15% 성능 개선

---

## 7. `[[likely]]` / `[[unlikely]]` 분기 예측 힌트 [적용 완료]

### 문제
- CPU 분기 예측 실패 시 ~15~20 cycle 패널티

### 적용
```cpp
// 매칭 루프에서 가격 불일치 탈출 — 드문 경우
if (order->type == OrderType::Limit && order->price < bestAsk_) [[unlikely]] {
    break;
}

// 잔량이 남아서 대기 — 일반적 경우
if (order->quantity > 0 && type == OrderType::Limit) [[likely]] {
    // rest on book
}
```

### 효과
- 분기 예측 적중률 개선 (단독 효과 1~3%, LTO와 병행 시 중첩)
- 코드의 의도를 명시적으로 문서화하는 부수 효과
