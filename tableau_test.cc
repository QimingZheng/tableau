#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "tableau.h"

typedef float T;

TEST(List, Append) {
  List<T> list;
  for (auto i = 0; i < 1024; i += 1) {
    EXPECT_EQ(list.At(i), 0);
    list.Append(i, i + 1);
    EXPECT_EQ(list.At(i), i + 1);
  }
}

TEST(List, Set) {
  List<T> list;
  for (auto i = 0; i < 10; i += 1) {
    list.Append(i, i + 1);
  }
  for (auto i = 0; i < 10; i += 1) {
    EXPECT_EQ(list.At(i), i + 1);
    list.Set(i, -1);
    EXPECT_EQ(list.At(i), -1);
  }
  for (auto i = 10; i < 20; i += 1) {
    list.Set(i, -1);
    EXPECT_EQ(list.At(i), 0);
  }
}

TEST(List, Iterator) {
  List<T> list;
  for (auto i = 0; i < 1024; i += 1) list.Append(i, i + 1);
  auto iter = list.Begin();
  while (!iter->IsEnd()) {
    EXPECT_EQ(iter->Index() + 1.0f, iter->Data());
    iter = iter->Next();
  }
}

T transform(const T &x) { return x + 1; }

TEST(List, Map) {
  List<T> list;
  for (auto i = 0; i < 1024; i += 1) list.Append(i, i + 1);
  auto new_list = list.Map(transform);
  for (auto i = 0; i < 1024; i += 1) EXPECT_EQ(new_list->At(i), i + 2);
}

List<T>::ReduceStruct reduce(const List<T>::ReduceStruct &x,
                             const List<T>::ReduceStruct &y) {
  if (x.second < y.second) return x;
  return y;
}

TEST(List, Reduce) {
  List<T> list;
  for (auto i = 0; i < 1024; i += 1) list.Append(i, i + 1);
  auto result = list.Reduce(reduce, {-1, 100000000.0f});
  EXPECT_EQ(result.first, 0);
  EXPECT_EQ(result.second, 1.0f);
}

TEST(List, Add) {
  List<T> list1, list2;
  for (auto i = 0; i < 1024; i += 1) {
    EXPECT_EQ(list1.At(2 * i), 0);
    list1.Append(2 * i, i + 1);
    EXPECT_EQ(list1.At(2 * i), i + 1);

    EXPECT_EQ(list2.At(2 * i + 1), 0);
    list2.Append(2 * i + 1, i + 1);
    EXPECT_EQ(list2.At(2 * i + 1), i + 1);
  }
  list1.Add(&list2);
  for (auto i = 0; i < 1024; i++) {
    EXPECT_EQ(list1.At(2 * i), i + 1);
    EXPECT_EQ(list1.At(2 * i + 1), i + 1);
  }
}

TEST(List, Add2) {
  List<T> list1, list2;
  for (auto i = 0; i < 1024; i += 1) list1.Append(512 + i, i % 2);
  for (auto i = 0; i < 512; i += 1) list2.Append(i, i % 2);

  list1.Add(&list2);

  for (auto i = 0; i < 1024 + 512; i++) {
    EXPECT_EQ(list1.At(i), i % 2);
  }
}

TEST(List, Product) {
  List<T> list1, list2;
  for (auto i = 0; i < 1024; i += 1) {
    list1.Append(2 * i, i + 1);

    list2.Append(2 * i + 1, i + 1);
  }
  EXPECT_EQ(list1.Dot(&list2), 0.0f);
}

TEST(List, Produc2) {
  List<T> list1, list2;
  for (auto i = 0; i < 1024; i += 1) {
    list1.Append(2 * i, 1);

    list2.Append(2 * i, 1);
  }
  EXPECT_EQ(list1.Dot(&list2), 1024.0f);
}

TEST(List, Cross) {
  List<T> list1, list2;
  for (auto i = 0; i < 16; i += 1) {
    list1.Append(i, i);
    list2.Append(i, i);
  }
  Tableau<T> *result = list1.Cross(&list2, 16, 16);
  for (auto i = 0; i < 16; i++) {
    for (auto j = 0; j < 16; j++) {
      EXPECT_EQ(result->At(i, j), i * j);
    }
  }
}

TEST(Tableau, Add) {
  List<T> list1, list2;
  for (auto i = 0; i < 16; i += 1) {
    list1.Append(i, 1);
    list2.Append(i, i);
  }
  Tableau<T> *tableau1 = list1.Cross(&list2, 16, 16);
  Tableau<T> *tableau2 = list1.Cross(&list1, 16, 16);
  tableau1->Add(tableau2);
  for (auto i = 0; i < 16; i++) {
    for (auto j = 0; j < 16; j++) {
      EXPECT_EQ(tableau1->At(i, j), j + 1);
    }
  }
}