#pragma once

#include <assert.h>
#include <omp.h>
#include <pthread.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#define assert_msg(cond, fmt, ...) \
  assert(cond || !fprintf(stderr, fmt, ##__VA_ARGS__))

typedef int64_t tableau_index_t;
typedef int64_t tableau_size_t;

enum TableauStorageFormat {
  ROW_ONLY,
  COLUMN_ONLY,
  ROW_AND_COLUMN,
};

enum ListStorageFormat {
  DENSE,
  SPARSE,
};

template <typename T>
class SparseTableau;

template <typename T>
class Tableau;

template <typename T>
inline bool _IsZeroT(const T& value);

/**
 * A list is the abstraction of a row or column in a simplex tableau.
 */
template <typename T>
class List {
 public:
  class Iterator {
   public:
    Iterator(List<T>* list) : list_(list) { index_ = 0; }

    tableau_index_t Index() {
      if (list_->StorageFormat() == SPARSE)
        return list_->index_[index_];
      else
        return index_;
    }
    T Data() { return list_->data_[index_]; }

    Iterator* Next() {
      index_ += 1;
      return this;
    }

    bool IsEnd() { return index_ >= list_->size_; }

   private:
    List<T>* list_;
    tableau_index_t index_;
  };

  Iterator* Begin() { return new Iterator(this); }

  List(tableau_size_t size = 0, ListStorageFormat format = SPARSE)
      : storage_format_(format) {
    if (format == SPARSE) {
      capacity_ = 1;
      while (capacity_ < size) {
        capacity_ <<= 1;
      }
      size_ = 0;
      index_ = new tableau_index_t[capacity_];
      data_ = new T[capacity_];
    } else {
      size_ = size;
      capacity_ = size;
      if (size > 0) {
        data_ = new T[capacity_];
        for (auto i = 0; i < capacity_; i++) data_[i] = 0;
      }
    }
  }
  ~List() {
    if (storage_format_ == SPARSE) {
      if (capacity_ > 0) {
        delete data_;
        delete index_;
      }
    } else {
      if (capacity_ > 0) {
        delete data_;
      }
    }
    data_ = nullptr;
    index_ = nullptr;
    size_ = 0;
    capacity_ = 0;
  }

  List(const List<T>* other) {
    storage_format_ = other->storage_format_;
    if (storage_format_ == SPARSE) {
      size_ = other->size_;
      capacity_ = other->capacity_;
      index_ = new tableau_index_t[capacity_];
      data_ = new T[capacity_];
      std::memcpy(index_, other->index_, sizeof(tableau_index_t) * capacity_);
      std::memcpy(data_, other->data_, sizeof(T) * capacity_);
    } else {
      size_ = other->size_;
      capacity_ = other->capacity_;
      data_ = new T[capacity_];
      std::memcpy(data_, other->data_, sizeof(T) * capacity_);
    }
  }

  ListStorageFormat StorageFormat() const { return storage_format_; }

  void Clear() { size_ = 0; }

  T At(tableau_index_t index) {
    if (storage_format_ == SPARSE) {
      tableau_index_t pos = BinarySearch(index);
      if (pos >= 0) return data_[pos];
      return 0;
    } else {
      return data_[index];
    }
  }

  /* Set the element at index, if no element is at index, skip. */
  void Set(tableau_index_t index, T value) {
    if (storage_format_ == SPARSE) {
      tableau_index_t pos = BinarySearch(index);
      if (pos >= 0) data_[pos] = value;
    } else {
      data_[index] = value;
    }
  }

  void Erase(tableau_index_t index) {
    assert(StorageFormat() == SPARSE);
    tableau_index_t pos = BinarySearch(index);
    if (pos >= 0) {
      for (auto i = pos + 1; i < size_; i++) {
        data_[i - 1] = data_[i];
        index_[i - 1] = index_[i] - 1;
      }
      size_ -= 1;
    }
  }

  void Add(const List<T>* other) { AddScaled(other, 1, false); }
  void AddScaled(const List<T>* other, T scale, bool enable_scale) {
    if (StorageFormat() == SPARSE and other->StorageFormat() == SPARSE) {
      SparseAdd(other, scale, enable_scale);
    } else if (StorageFormat() == DENSE and other->StorageFormat() == DENSE) {
      assert_msg(Size() == other->Size(),
                 "Cannot add two lists with different size");
      if (enable_scale)
        for (auto i = 0; i < Size(); i++) data_[i] += scale * other->data_[i];
      else
        for (auto i = 0; i < Size(); i++) data_[i] += other->data_[i];
    } else {
      // case 1: StorageFormat() == SPARSE and other->StorageFormat() == DENSE
      // case 2: StorageFormat() == DENSE and other->StorageFormat() == SPARSE
      tableau_index_t* sparse_index =
          StorageFormat() == SPARSE ? index_ : other->index_;
      tableau_size_t dense_size =
          StorageFormat() == DENSE ? Size() : other->Size();
      tableau_index_t sparse_size =
          StorageFormat() == SPARSE ? Size() : other->Size();
      if (sparse_size > 0) assert(sparse_index[sparse_size - 1] < dense_size);
      T* dense_data = StorageFormat() == DENSE ? data_ : other->data_;
      T* sparse_data = StorageFormat() == SPARSE ? data_ : other->data_;
      if (StorageFormat() == DENSE) {
        if (enable_scale)
          for (auto i = 0; i < sparse_size; i++)
            dense_data[sparse_index[i]] += scale * sparse_data[i];
        else
          for (auto i = 0; i < sparse_size; i++)
            dense_data[sparse_index[i]] += sparse_data[i];
      } else {
        T* new_data = new T[dense_size];
        std::memcpy(new_data, dense_data, sizeof(T) * dense_size);
        if (enable_scale)
          for (auto i = 0; i < dense_size; i++) new_data[i] *= scale;
        if (enable_scale)
          for (auto i = 0; i < sparse_size; i++)
            new_data[sparse_index[i]] += sparse_data[i];
        else
          for (auto i = 0; i < sparse_size; i++)
            new_data[sparse_index[i]] += sparse_data[i];
        delete data_;
        delete index_;
        data_ = new_data;
        index_ = nullptr;
        size_ = dense_size;
        capacity_ = dense_size;
        storage_format_ = DENSE;
      }
    }
  }

  void Mul(const List<T>* other) {
    if (StorageFormat() == SPARSE and other->StorageFormat() == SPARSE) {
      SparseMul(other);
    } else if (StorageFormat() == DENSE and other->StorageFormat() == DENSE) {
      assert_msg(Size() == other->Size(),
                 "Cannot add two lists with different size");
      for (auto i = 0; i < Size(); i++) {
        data_[i] *= other->data_[i];
      }
    } else {
      // case 1: StorageFormat() == SPARSE and other->StorageFormat() == DENSE
      // case 2: StorageFormat() == DENSE and other->StorageFormat() == SPARSE
      tableau_index_t* sparse_index =
          StorageFormat() == SPARSE ? index_ : other->index_;
      tableau_size_t dense_size =
          StorageFormat() == DENSE ? Size() : other->Size();
      tableau_index_t sparse_size =
          StorageFormat() == SPARSE ? Size() : other->Size();
      if (sparse_size > 0) assert(sparse_index[sparse_size - 1] < dense_size);
      T* dense_data = StorageFormat() == DENSE ? data_ : other->data_;
      T* sparse_data = StorageFormat() == SPARSE ? data_ : other->data_;
      if (StorageFormat() == SPARSE) {
        for (auto i = 0; i < sparse_size; i++) {
          sparse_data[i] *= dense_data[sparse_index[i]];
        }
      } else {
        T* new_data = new T[sparse_size];
        tableau_index_t* new_index = new tableau_index_t[sparse_size];
        std::memcpy(new_data, sparse_data, sizeof(T) * sparse_size);
        std::memcpy(new_index, sparse_index,
                    sizeof(tableau_index_t) * sparse_size);
        for (auto i = 0; i < sparse_size; i++) {
          new_data[i] += dense_data[sparse_index[i]];
        }
        delete data_;
        data_ = new_data;
        index_ = new_index;
        size_ = sparse_size;
        capacity_ = sparse_size;
        storage_format_ = SPARSE;
      }
    }
  }

  void Scale(const T scale) {
    for (tableau_index_t i = 0; i < size_; i++) data_[i] *= scale;
  }

  template <typename R>
  List<R>* Map(
      std::function<R(const tableau_index_t&, const T&)> transform) const {
    List<R>* list = new List<R>(Size(), StorageFormat());
    if (StorageFormat() == SPARSE) {
      for (tableau_index_t i = 0; i < Size(); i++) list->Append(index_[i], 0);
#pragma omp parallel for
      for (tableau_index_t i = 0; i < Size(); i++)
        list->Set(index_[i], transform(index_[i], data_[i]));
    } else {
#pragma omp parallel for
      for (tableau_index_t i = 0; i < Size(); i++) {
        list->Set(i, transform(i, data_[i]));
      }
    }
    return list;
  }

  template <typename R>
  List<R>* Map(R (*transform)(const T&)) const {
    List<R>* list = new List<R>(Size(), StorageFormat());
    if (StorageFormat() == SPARSE) {
#pragma omp parallel for
      for (tableau_index_t i = 0; i < Size(); i++) {
        list->index_[i] = index_[i];
        list->data_[i] = transform(data_[i]);
      }
      list->size_ = Size();
    } else {
#pragma omp parallel for
      for (tableau_index_t i = 0; i < Size(); i++) {
        list->data_[i] = transform(data_[i]);
      }
      list->size_ = Size();
    }
    return list;
  }

  typedef std::pair<tableau_index_t, T> ReduceStruct;
  static ReduceStruct MaxAbsReduce(const ReduceStruct& a,
                                   const ReduceStruct& b) {
    return (std::abs(a.second) < std::abs(b.second)) ? b : a;
  }
  static ReduceStruct MinReduce(const ReduceStruct& a, const ReduceStruct& b) {
    return (b.second < a.second) ? b : a;
  }

  ReduceStruct Reduce(ReduceStruct (*reduce)(const ReduceStruct&,
                                             const ReduceStruct&),
                      ReduceStruct initial_value) const {
    if (StorageFormat() == SPARSE) {
      for (tableau_index_t i = 0; i < Size(); i++) {
        initial_value = reduce({index_[i], data_[i]}, initial_value);
      }
      return initial_value;
    } else {
      for (tableau_index_t i = 0; i < Size(); i++) {
        initial_value = reduce({i, data_[i]}, initial_value);
      }
      return initial_value;
    }
  }

  Tableau<T>* Cross(const List<T>* other, tableau_size_t rows,
                    tableau_size_t cols,
                    TableauStorageFormat format = ROW_AND_COLUMN) const;

  SparseTableau<T>* SparseCross(
      const List<T>* other, TableauStorageFormat format = ROW_AND_COLUMN) const;

  tableau_size_t Size() const { return size_; }

  void Append(tableau_index_t index, T value) {
    if (StorageFormat() == SPARSE) {
      if (size_ >= capacity_) {
        capacity_ *= 2;
        T* new_data = new T[capacity_];
        tableau_index_t* new_index = new tableau_index_t[capacity_];

        std::memcpy(new_index, index_, sizeof(tableau_index_t) * size_);
        std::memcpy(new_data, data_, sizeof(T) * size_);

        delete index_;
        delete data_;
        index_ = new_index;
        data_ = new_data;
      }
      index_[size_] = index;
      data_[size_] = value;
      size_ += 1;
    } else {
      assert(index < size_);
      data_[index] = value;
    }
  }

  T Dot(const List<T>* other) const {
    if (StorageFormat() == SPARSE and other->StorageFormat() == SPARSE) {
      return SparseDot(other);
    } else if (StorageFormat() == DENSE and other->StorageFormat() == DENSE) {
      assert_msg(Size() == other->Size(),
                 "Cannot Dot two Dense Lists with different size");
      T product = 0;
      for (tableau_index_t i = 0; i < size_; i++) {
        product += data_[i] * other->data_[i];
      }
      return product;
    } else {
      // case 1: StorageFormat() == SPARSE and other->StorageFormat() == DENSE
      // case 2: StorageFormat() == DENSE and other->StorageFormat() == SPARSE
      tableau_index_t* sparse_index =
          StorageFormat() == SPARSE ? index_ : other->index_;
      tableau_size_t dense_size =
          StorageFormat() == DENSE ? Size() : other->Size();
      tableau_index_t sparse_size =
          StorageFormat() == SPARSE ? Size() : other->Size();
      if (sparse_size > 0) assert(sparse_index[sparse_size - 1] < dense_size);
      T* dense_data = StorageFormat() == DENSE ? data_ : other->data_;
      T* sparse_data = StorageFormat() == SPARSE ? data_ : other->data_;
      T product = 0;
      for (tableau_index_t i = 0; i < sparse_size; i++) {
        product += sparse_data[i] * dense_data[sparse_index[i]];
      }
      return product;
    }
  }
  void Pop(tableau_index_t last_index = -1) {
    assert_msg(StorageFormat() == SPARSE, "Dense List does not support Pop");
    if (size_ > 0) {
      if (last_index > 0) {
        if (index_[size_ - 1] != last_index) return;
      }
      size_ -= 1;
    }
  }

  friend class Iterator;

  template <typename U>
  friend class SparseTableau;

  template <typename U>
  friend class List;

 private:
  tableau_size_t size_ = 0;
  tableau_size_t capacity_ = 0;
  tableau_index_t* index_ = nullptr;
  T* data_ = nullptr;
  ListStorageFormat storage_format_ = SPARSE;

  void SparseAdd(const List<T>* other, T scale, bool enable_scale) {
    if (other->Size() == 0) return;
    capacity_ = Size() + other->Size();
    tableau_index_t* merged_index = new tableau_index_t[capacity_];
    T* merged_data = new T[capacity_];
    tableau_index_t left_index = 0, right_index = 0, next_index = 0;

    while (left_index < Size() && right_index < other->Size()) {
      if (index_[left_index] == other->index_[right_index]) {
        T sum = data_[left_index] + (enable_scale
                                         ? scale * other->data_[right_index]
                                         : other->data_[right_index]);
        if (!_IsZeroT(sum)) {
          merged_data[next_index] = sum;
          merged_index[next_index] = index_[left_index];
          next_index++;
        }
        left_index++;
        right_index++;
      } else if (index_[left_index] < other->index_[right_index]) {
        if (!_IsZeroT(data_[left_index])) {
          merged_data[next_index] = data_[left_index];
          merged_index[next_index] = index_[left_index];
          next_index++;
        }
        left_index++;
      } else {
        if (!_IsZeroT(scale * other->data_[right_index])) {
          merged_data[next_index] =
              (enable_scale ? scale * other->data_[right_index]
                            : other->data_[right_index]);
          merged_index[next_index] = other->index_[right_index];
          next_index++;
        }
        right_index++;
      }
    }
    while (left_index < Size()) {
      if (!_IsZeroT(data_[left_index])) {
        merged_data[next_index] = data_[left_index];
        merged_index[next_index] = index_[left_index];
        next_index++;
      }
      left_index++;
    }
    while (right_index < other->Size()) {
      if (!_IsZeroT(scale * other->data_[right_index])) {
        merged_data[next_index] =
            (enable_scale ? scale * other->data_[right_index]
                          : other->data_[right_index]);
        merged_index[next_index] = other->index_[right_index];
        next_index++;
      }
      right_index++;
    }
    size_ = next_index;
    delete index_;
    delete data_;
    index_ = merged_index;
    data_ = merged_data;
  }
  void SparseMul(const List<T>* other) {
    capacity_ = std::min(Size(), other->Size());
    tableau_index_t* merged_index = new tableau_index_t[capacity_];
    T* merged_data = new T[capacity_];
    tableau_index_t left_index = 0, right_index = 0, next_index = 0;

    while (left_index < Size() && right_index < other->Size()) {
      if (index_[left_index] == other->index_[right_index]) {
        T prod = data_[left_index] * other->data_[right_index];
        if (!_IsZeroT(prod)) {
          merged_data[next_index] = prod;
          merged_index[next_index] = index_[left_index];
          next_index++;
        }
        left_index++;
        right_index++;
      } else if (index_[left_index] < other->index_[right_index]) {
        left_index++;
      } else {
        right_index++;
      }
    }
    size_ = next_index;
    delete index_;
    delete data_;
    index_ = merged_index;
    data_ = merged_data;
  }

  T SparseDot(const List<T>* other) const {
    tableau_index_t left = 0, right = 0;
    T product = 0;
    while (left < Size() and right < other->Size()) {
      if (index_[left] == other->index_[right]) {
        product += data_[left] * other->data_[right];
        left += 1;
        right += 1;
      } else if (index_[left] < other->index_[right])
        left += 1;
      else
        right += 1;
    }
    return product;
  }

  tableau_index_t BinarySearch(tableau_index_t index) {
    tableau_index_t lower = 0, upper = size_ - 1;
    while (lower <= upper) {
      tableau_index_t middle = (lower + upper) / 2;
      if (index_[middle] == index) {
        return middle;
      }
      if (index_[middle] > index) {
        upper = middle - 1;
      } else {
        lower = middle + 1;
      }
    }
    return -1;
  }
};

/* A Simplex Tableau. */
template <typename T>
class Tableau {
 public:
  Tableau(tableau_size_t rows, tableau_size_t columns,
          TableauStorageFormat format = ROW_AND_COLUMN)
      : rows_(rows), columns_(columns), storage_format_(format) {
    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN) {
      row_heads_ = new List<T>*[rows];
#pragma omp parallel for
      for (tableau_size_t i = 0; i < rows; i++) row_heads_[i] = new List<T>();
    }
    if (storage_format_ == COLUMN_ONLY or storage_format_ == ROW_AND_COLUMN) {
      col_heads_ = new List<T>*[columns];
#pragma omp parallel for
      for (tableau_size_t i = 0; i < columns; i++)
        col_heads_[i] = new List<T>();
    }
  }
  ~Tableau() {
    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN) {
      for (auto i = 0; i < rows_; i++) {
        delete row_heads_[i];
      }
      delete row_heads_;
    }
    if (storage_format_ == COLUMN_ONLY or storage_format_ == ROW_AND_COLUMN) {
      for (auto i = 0; i < columns_; i++) {
        delete col_heads_[i];
      }
      delete col_heads_;
    }
  }

  T At(tableau_index_t row, tableau_index_t col) {
    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN)
      return Row(row)->At(col);
    else
      return Col(col)->At(row);
  }

  List<T>* Row(tableau_index_t row) const {
    if (storage_format_ == COLUMN_ONLY) {
      throw std::runtime_error(
          "Cannot get row of tableau in column only storage format");
    }
    return row_heads_[row];
  }
  List<T>* Col(tableau_index_t col) const {
    if (storage_format_ == ROW_ONLY) {
      throw std::runtime_error(
          "Cannot get column of tableau in row only storage format");
    }
    return col_heads_[col];
  }

  void Add(const Tableau<T>* other) {
    assert(rows_ == other->rows_);
    assert(columns_ == other->columns_);
    assert(storage_format_ == other->storage_format_);

    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN) {
#pragma omp parallel for
      for (tableau_index_t row = 0; row < rows_; row++)
        Row(row)->Add(other->Row(row));
    }
    if (storage_format_ == COLUMN_ONLY or storage_format_ == ROW_AND_COLUMN) {
#pragma omp parallel for
      for (tableau_index_t col = 0; col < columns_; col++)
        Col(col)->Add(other->Col(col));
    }
  }

  void Add(const SparseTableau<T>* other) {
    assert(storage_format_ == other->StorageFormat());

    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN) {
#pragma omp parallel for
      for (tableau_index_t row = 0; row < other->Rows(); row++)
        Row(other->SparseRowIndexOf(row))->Add(other->Row(row));
    }
    if (storage_format_ == COLUMN_ONLY or storage_format_ == ROW_AND_COLUMN) {
#pragma omp parallel for
      for (tableau_index_t col = 0; col < other->Cols(); col++)
        Col(other->SparseColIndexOf(col))->Add(other->Col(col));
    }
  }

  template <typename U>
  friend class List;

  void AppendRow(tableau_index_t row, List<T>* list) {
    // TODO: delete row_heads_[row]
    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN) {
      row_heads_[row] = list;
    }
    if (storage_format_ == COLUMN_ONLY or storage_format_ == ROW_AND_COLUMN) {
      for (auto iter = list->Begin(); iter->IsEnd() == false;
           iter = iter->Next()) {
        Col(iter->Index())->Append(row, iter->Data());
      }
    }
  }
  void AppendCol(tableau_index_t col, List<T>* list) {
    // TODO: delete col_heads_[col]
    if (storage_format_ == COLUMN_ONLY or storage_format_ == ROW_AND_COLUMN) {
      col_heads_[col] = list;
    }
    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN) {
      for (auto iter = list->Begin(); iter->IsEnd() == false;
           iter = iter->Next()) {
        Row(iter->Index())->Append(col, iter->Data());
      }
    }
  }

  void AppendExtraCol(List<T>* list) {
    columns_ += 1;
    if (storage_format_ == COLUMN_ONLY or storage_format_ == ROW_AND_COLUMN) {
      List<T>** new_col_heads = new List<T>*[columns_];
      for (auto i = 0; i < columns_ - 1; i++) new_col_heads[i] = col_heads_[i];
      new_col_heads[columns_ - 1] = list;
      delete col_heads_;
      col_heads_ = new_col_heads;
    }
    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN) {
      for (auto iter = list->Begin(); iter->IsEnd() == false;
           iter = iter->Next()) {
        Row(iter->Index())->Append(columns_ - 1, iter->Data());
      }
    }
  }
  void RemoveExtraCol() {
    columns_ -= 1;
    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN) {
      auto last_col = col_heads_[columns_];
      for (auto iter = last_col->Begin(); iter->IsEnd() == false;
           iter = iter->Next()) {
        Row(iter->Index())->Pop(columns_);
      }
    }
  }
  List<T>* SumScaledRows(List<T>* scale) {
    List<T>* ret = new List<T>(columns_, DENSE);
    if (StorageFormat() == ROW_ONLY or StorageFormat() == ROW_AND_COLUMN) {
      for (auto iter = scale->Begin(); !iter->IsEnd(); iter = iter->Next())
        ret->AddScaled(Row(iter->Index()), iter->Data(), true);
      return ret;
    } else {
#pragma omp parallel
      for (tableau_index_t col = 0; col < columns_; col++)
        ret->Set(col, scale->Dot(Col(col)));
      return ret;
    }
  }
  List<T>* Times(List<T>* x) {
    assert_msg(StorageFormat() == ROW_ONLY or StorageFormat() == ROW_AND_COLUMN,
               "Scale List must be in Dense format");
    if (x->StorageFormat() == DENSE) assert(Cols() == x->Size());
    List<T>* ret = new List<T>(rows_, DENSE);
#pragma omp parallel
    for (tableau_index_t row = 0; row < rows_; row++)
      ret->Set(row, Row(row)->Dot(x));
    return ret;
  }

  tableau_size_t Rows() const { return rows_; }
  tableau_size_t Cols() const { return columns_; }
  TableauStorageFormat StorageFormat() const { return storage_format_; }

 private:
  void SetRow(tableau_index_t row, List<T>* list) {
    if (storage_format_ == COLUMN_ONLY) {
      throw std::runtime_error(
          "Cannot call SetRow for tableau in column only storage format");
    }
    delete row_heads_[row];
    row_heads_[row] = list;
  }
  void SetCol(tableau_index_t col, List<T>* list) {
    if (storage_format_ == ROW_ONLY) {
      throw std::runtime_error(
          "Cannot call SetCol for tableau in row only storage format");
    }
    delete col_heads_[col];
    col_heads_[col] = list;
  }
  tableau_size_t rows_, columns_;
  List<T>** row_heads_;
  List<T>** col_heads_;
  TableauStorageFormat storage_format_ = ROW_AND_COLUMN;
};

template <typename T>
Tableau<T>* List<T>::Cross(const List<T>* other, tableau_size_t rows,
                           tableau_size_t cols,
                           TableauStorageFormat format) const {
  Tableau<T>* tableau = new Tableau<T>(rows, cols, format);
  if (format == ROW_ONLY or format == ROW_AND_COLUMN) {
#pragma omp parallel for
    for (tableau_index_t i = 0; i < Size(); i++) {
      List<T>* row = new List<T>(other);
      tableau_index_t index = index_[i];
      T scale = data_[i];
      row->Scale(scale);
      tableau->SetRow(index, row);
    }
  }
  if (format == COLUMN_ONLY or format == ROW_AND_COLUMN) {
#pragma omp parallel for
    for (tableau_index_t i = 0; i < other->Size(); i++) {
      List<T>* col = new List<T>(this);
      tableau_index_t index = other->index_[i];
      T scale = other->data_[i];
      col->Scale(scale);
      tableau->SetCol(index, col);
    }
  }
  return tableau;
}

template <typename T>
class SparseTableau {
 public:
  SparseTableau(tableau_size_t rows, tableau_size_t cols,
                TableauStorageFormat format)
      : storage_format_(format) {
    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN) {
      if (rows > 0) {
        sparse_row_heads_ = new List<List<T>*>(rows);
        sparse_row_heads_->size_ = rows;
      }
    }
    if (storage_format_ == COLUMN_ONLY or storage_format_ == ROW_AND_COLUMN) {
      if (cols > 0) {
        sparse_col_heads_ = new List<List<T>*>(cols);
        sparse_col_heads_->size_ = cols;
      }
    }
  }
  ~SparseTableau() {
    if (storage_format_ == ROW_ONLY or storage_format_ == ROW_AND_COLUMN) {
      if (sparse_row_heads_ != nullptr) {
        for (auto i = 0; i < sparse_row_heads_->Size(); i++) {
          delete sparse_row_heads_->data_[i];
        }
        delete sparse_row_heads_;
      }
    }
    if (storage_format_ == COLUMN_ONLY or storage_format_ == ROW_AND_COLUMN) {
      if (sparse_col_heads_ != nullptr) {
        for (auto i = 0; i < sparse_col_heads_->Size(); i++) {
          delete sparse_col_heads_->data_[i];
        }
        delete sparse_col_heads_;
      }
    }
  }

  List<T>* Row(tableau_index_t row) const {
    CheckFormat(COLUMN_ONLY, "Row");
    return sparse_row_heads_->data_[row];
  }
  List<T>* Col(tableau_index_t col) const {
    CheckFormat(ROW_ONLY, "Col");
    return sparse_col_heads_->data_[col];
  }
  tableau_index_t SparseRowIndexOf(tableau_index_t row) const {
    CheckFormat(COLUMN_ONLY, "SparseRowIndexOf");
    return sparse_row_heads_->index_[row];
  }
  tableau_index_t SparseColIndexOf(tableau_index_t col) const {
    CheckFormat(ROW_ONLY, "SparseColIndexOf");
    return sparse_col_heads_->index_[col];
  }

  tableau_size_t Rows() const {
    CheckFormat(COLUMN_ONLY, "Rows");
    return sparse_row_heads_->Size();
  }
  tableau_size_t Cols() const {
    CheckFormat(ROW_ONLY, "Cols");
    return sparse_col_heads_->Size();
  }

  TableauStorageFormat StorageFormat() const { return storage_format_; }

  template <typename U>
  friend class List;

 private:
  void SetRow(tableau_index_t row, tableau_index_t sparse_row_index,
              List<T>* list) {
    CheckFormat(COLUMN_ONLY, "SetRow");
    sparse_row_heads_->index_[row] = sparse_row_index;
    sparse_row_heads_->data_[row] = list;
  }
  void SetCol(tableau_index_t col, tableau_index_t sparse_col_index,
              List<T>* list) {
    CheckFormat(ROW_ONLY, "SetCol");
    sparse_col_heads_->index_[col] = sparse_col_index;
    sparse_col_heads_->data_[col] = list;
  }
  inline void CheckFormat(TableauStorageFormat unexpected_format,
                          const char* method_name) const {
    assert_msg(storage_format_ != unexpected_format,
               "Cannot call %s when the sparse tableau is in %d storage format",
               method_name, storage_format_);
  }

  List<List<T>*>* sparse_row_heads_ = nullptr;
  List<List<T>*>* sparse_col_heads_ = nullptr;
  TableauStorageFormat storage_format_ = ROW_AND_COLUMN;
};

template <typename T>
SparseTableau<T>* List<T>::SparseCross(const List<T>* other,
                                       TableauStorageFormat format) const {
  SparseTableau<T>* sparse_tableau =
      new SparseTableau<T>(Size(), other->Size(), format);
  if (format == ROW_ONLY or format == ROW_AND_COLUMN) {
#pragma omp parallel for
    for (tableau_index_t i = 0; i < Size(); i++) {
      List<T>* row = new List<T>(other);
      tableau_index_t index = (StorageFormat() == SPARSE) ? index_[i] : i;
      T scale = data_[i];
      row->Scale(scale);
      sparse_tableau->SetRow(i, index, row);
    }
  }
  if (format == COLUMN_ONLY or format == ROW_AND_COLUMN) {
#pragma omp parallel for
    for (tableau_index_t i = 0; i < other->Size(); i++) {
      List<T>* col = new List<T>(this);
      tableau_index_t index =
          (StorageFormat() == SPARSE) ? other->index_[i] : i;
      T scale = other->data_[i];
      col->Scale(scale);
      sparse_tableau->SetCol(i, index, col);
    }
  }
  return sparse_tableau;
}
