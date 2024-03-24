#include <benchmark/benchmark.h>

#include "tableau.h"

typedef float T;

static void CustomArguments(benchmark::internal::Benchmark* b) {
  for (tableau_size_t sparse_element_size = 1; sparse_element_size <= 1000;
       sparse_element_size = sparse_element_size * 10)
    for (tableau_size_t sparse_array_size = 10 * sparse_element_size;
         sparse_array_size < sparse_element_size * 100000000;
         sparse_array_size *= 10)
      b->Args({sparse_element_size, sparse_array_size});
}

static void List_Append(benchmark::State& state) {
  tableau_size_t sparse_element_size = state.range(0);
  tableau_size_t sparse_array_size = state.range(1);
  List<T>* list = new List<T>(sparse_element_size);
  for (auto _ : state) {
    for (auto i = 0; i < sparse_element_size; i++) {
      list->Append(i * (sparse_array_size / sparse_element_size), 1.0);
    }
    list->Clear();
  }
  delete list;
}
BENCHMARK(List_Append)->Apply(CustomArguments);

static void List_At(benchmark::State& state) {
  tableau_size_t sparse_element_size = state.range(0);
  tableau_size_t sparse_array_size = state.range(1);
  List<T>* list = new List<T>(sparse_element_size);
  for (auto i = 0; i < sparse_element_size; i++) {
    list->Append(i * (sparse_array_size / sparse_element_size), 1.0);
  }
  for (auto _ : state) {
    for (auto i = 0; i < sparse_element_size; i++) {
      list->At(i * (sparse_array_size / sparse_element_size));
    }
  }
  delete list;
}
BENCHMARK(List_At)->Apply(CustomArguments);

static void List_Add(benchmark::State& state) {
  tableau_size_t sparse_element_size = 2048;
  List<T>* list1 = new List<T>(sparse_element_size);
  List<T>* list2 = new List<T>(sparse_element_size);
  for (auto i = 0; i < sparse_element_size; i++) {
    list1->Append(i * 2, i);
    list2->Append(i * 2 + 1, i);
  }
  for (auto _ : state) {
    list1->Add(list2);
  }
  delete list1;
  delete list2;
}
BENCHMARK(List_Add);

static void List_Mul(benchmark::State& state) {
  tableau_size_t sparse_element_size = 2048;
  List<T>* list1 = new List<T>(sparse_element_size);
  List<T>* list2 = new List<T>(sparse_element_size);
  for (auto i = 0; i < sparse_element_size; i++) {
    list1->Append(i, i);
    list2->Append(sparse_element_size / 2 + i, i);
  }
  for (auto _ : state) {
    list1->Mul(list2);
  }
  delete list1;
  delete list2;
}
BENCHMARK(List_Mul);

static void List_Scale(benchmark::State& state) {
  tableau_size_t sparse_element_size = 2048;
  List<T>* list = new List<T>(sparse_element_size);
  for (auto i = 0; i < sparse_element_size; i++) {
    list->Append(i, i);
  }
  for (auto _ : state) {
    list->Scale(1.0);
  }
  delete list;
}
BENCHMARK(List_Scale);

static void List_Dot(benchmark::State& state) {
  tableau_size_t sparse_element_size = 2048;
  List<T>* list1 = new List<T>(sparse_element_size);
  List<T>* list2 = new List<T>(sparse_element_size);
  for (auto i = 0; i < sparse_element_size; i++) {
    list1->Append(i, i);
    list2->Append(sparse_element_size / 2 + i, i);
  }
  for (auto _ : state) {
    list1->Dot(list2);
  }
  delete list1;
  delete list2;
}
BENCHMARK(List_Dot);

static void List_Reduce(benchmark::State& state) {
  tableau_size_t sparse_element_size = state.range(0);
  tableau_size_t sparse_array_size = state.range(1);
  List<T>* list = new List<T>(sparse_element_size);
  for (auto i = 0; i < sparse_element_size; i++) {
    list->Append(i * (sparse_array_size / sparse_element_size), i);
  }
  for (auto _ : state) {
    list->Reduce(List<T>::MinReduce, {-1, 100000.0});
  }
  delete list;
}
BENCHMARK(List_Reduce)->Apply(CustomArguments);

static void List_Cross(benchmark::State& state) {
  tableau_size_t sparse_element_size = state.range(0);
  tableau_size_t sparse_array_size = state.range(1);
  List<T>* list1 = new List<T>(sparse_element_size);
  List<T>* list2 = new List<T>(sparse_element_size);
  for (auto i = 0; i < sparse_element_size; i++) {
    list1->Append(i * 8, i);
    list2->Append(i, i);
  }
  for (auto _ : state) {
    auto tableau =
        list1->Cross(list2, 8 * sparse_element_size, sparse_element_size);
    delete tableau;
  }
  delete list1;
  delete list2;
}
BENCHMARK(List_Cross)->Apply(CustomArguments);

static void CustomTableauArguments1(benchmark::internal::Benchmark* b) {
  for (tableau_size_t row = 1000; row <= 10000000; row = row * 10)
    for (tableau_size_t col = 1000; col <= 10000000; col *= 10)
      b->Args({row, col});
}

static void Tableau_Constructor(benchmark::State& state) {
  tableau_size_t row = state.range(0);
  tableau_size_t col = state.range(1);
  for (auto _ : state) {
    Tableau<T>* tableau = new Tableau<T>(row, col);
    delete tableau;
  }
}
BENCHMARK(Tableau_Constructor)->Apply(CustomTableauArguments1);

static void CustomTableauArguments2(benchmark::internal::Benchmark* b) {
  for (tableau_size_t row = 1000; row <= 10000000; row = row * 10)
    for (tableau_size_t col = 1000; col <= 10000000; col *= 10)
      for (tableau_size_t row_element_size = col / 1000;
           row_element_size < col / 100; row_element_size *= 10)
        for (tableau_size_t col_element_size = row / 1000;
             col_element_size < row / 100; col_element_size *= 10)
          b->Args({row, col, row_element_size, col_element_size});
}

static void Tableau_AppendRow(benchmark::State& state) {
  tableau_size_t row = state.range(0);
  tableau_size_t col = state.range(1);
  tableau_size_t row_element_size = state.range(2);
  tableau_size_t col_element_size = state.range(3);
  for (auto _ : state) {
    Tableau<T>* tableau = new Tableau<T>(row, col);
    for (auto i = 0; i < col_element_size; i++) {
      List<T>* list = new List<T>(row_element_size);
      for (auto i = 0; i < row_element_size; i++) {
        list->Append(i * (col / row_element_size), i);
      }
      tableau->AppendRow(i, list);
    }
    delete tableau;
  }
}
BENCHMARK(Tableau_AppendRow)->Apply(CustomTableauArguments2);

BENCHMARK_MAIN();