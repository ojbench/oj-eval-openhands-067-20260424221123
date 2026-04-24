
#include <iostream>
#include <optional>
#include <stdexcept>
#include <memory>

class RefCellError : public std::runtime_error {
public:
    explicit RefCellError(const std::string& message) : std::runtime_error(message) {}
    virtual ~RefCellError() = default;
};// Abstract class as base class

//invalidly call an immutable borrow
class BorrowError : public RefCellError {
public:
    explicit BorrowError(const std::string& message) : RefCellError(message) {}
};
//invalidly call a mutable borrow
class BorrowMutError : public RefCellError {
public:
    explicit BorrowMutError(const std::string& message) : RefCellError(message) {}
};
//still has refs when destructed
class DestructionError : public RefCellError {
public:
    explicit DestructionError(const std::string& message) : RefCellError(message) {}
};

template <typename T>
class RefCell {
private:
    T value;
    mutable int immutable_borrow_count;
    mutable bool mutable_borrow_active;

public:
    // Forward declarations
    class Ref;
    class RefMut;

    // Constructor
    explicit RefCell(const T& initial_value) : value(initial_value), immutable_borrow_count(0), mutable_borrow_active(false) {
    }
    
    explicit RefCell(T&& initial_value) : value(std::move(initial_value)), immutable_borrow_count(0), mutable_borrow_active(false) {
    }
    
    // Disable copying and moving for simplicity
    RefCell(const RefCell&) = delete;
    RefCell& operator=(const RefCell&) = delete;
    RefCell(RefCell&&) = delete;
    RefCell& operator=(RefCell&&) = delete;

    // Borrow methods
    Ref borrow() const {
        if (mutable_borrow_active) {
            throw BorrowError("Cannot borrow immutably while mutable borrow exists");
        }
        immutable_borrow_count++;
        return Ref(const_cast<RefCell<T>*>(this));
    }

    std::optional<Ref> try_borrow() const {
        if (mutable_borrow_active) {
            return std::nullopt;
        }
        immutable_borrow_count++;
        return Ref(const_cast<RefCell<T>*>(this));
    }

    RefMut borrow_mut() {
        if (mutable_borrow_active || immutable_borrow_count > 0) {
            throw BorrowMutError("Cannot borrow mutably while other borrows exist");
        }
        mutable_borrow_active = true;
        return RefMut(this);
    }

    std::optional<RefMut> try_borrow_mut() {
        if (mutable_borrow_active || immutable_borrow_count > 0) {
            return std::nullopt;
        }
        mutable_borrow_active = true;
        return RefMut(this);
    }

    // Inner classes for borrows
    class Ref {
    private:
        RefCell<T>* cell;

    public:
        Ref() : cell(nullptr) {
        }

        explicit Ref(RefCell<T>* cell_ptr) : cell(cell_ptr) {
        }

        ~Ref() {
            if (cell) {
                cell->immutable_borrow_count--;
            }
        }

        const T& operator*() const {
            return cell->value;
        }

        const T* operator->() const {
            return &(cell->value);
        }

        // Allow copying
        Ref(const Ref& other) : cell(other.cell) {
            if (cell) {
                cell->immutable_borrow_count++;
            }
        }
        
        Ref& operator=(const Ref& other) {
            if (this != &other) {
                if (cell) {
                    cell->immutable_borrow_count--;
                }
                cell = other.cell;
                if (cell) {
                    cell->immutable_borrow_count++;
                }
            }
            return *this;
        }

        // Allow moving
        Ref(Ref&& other) noexcept : cell(other.cell) {
            other.cell = nullptr;
        }

        Ref& operator=(Ref&& other) noexcept {
            if (this != &other) {
                if (cell) {
                    cell->immutable_borrow_count--;
                }
                cell = other.cell;
                other.cell = nullptr;
            }
            return *this;
        }
    };

    class RefMut {
    private:
        RefCell<T>* cell;

    public:
        RefMut() : cell(nullptr) {
        }

        explicit RefMut(RefCell<T>* cell_ptr) : cell(cell_ptr) {
        }

        ~RefMut() {
            if (cell) {
                cell->mutable_borrow_active = false;
            }
        }

        T& operator*() {
            return cell->value;
        }

        T* operator->() {
            return &(cell->value);
        }

        // Disable copying to ensure correct borrow rules
        RefMut(const RefMut&) = delete;
        RefMut& operator=(const RefMut&) = delete;

        // Allow moving
        RefMut(RefMut&& other) noexcept : cell(other.cell) {
            other.cell = nullptr;
        }

        RefMut& operator=(RefMut&& other) noexcept {
            if (this != &other) {
                if (cell) {
                    cell->mutable_borrow_active = false;
                }
                cell = other.cell;
                other.cell = nullptr;
            }
            return *this;
        }
    };

    // Destructor
    ~RefCell() noexcept(false) {
        if (immutable_borrow_count > 0 || mutable_borrow_active) {
            throw DestructionError("RefCell destroyed while borrows are still active");
        }
    }
};

// Simple test function
int main() {
    try {
        RefCell<int> cell(42);
        
        // Test basic borrowing
        {
            auto ref1 = cell.borrow();
            std::cout << "Value: " << *ref1 << std::endl;
            
            auto ref2 = cell.try_borrow();
            if (ref2) {
                std::cout << "Try borrow succeeded: " << **ref2 << std::endl;
            }
        } // ref1 and ref2 go out of scope here
        
        // Test mutable borrowing
        {
            auto mut_ref = cell.borrow_mut();
            *mut_ref = 100;
            std::cout << "Modified value: " << *mut_ref << std::endl;
        } // mut_ref goes out of scope here
        
        // Test after mutable borrow is done
        auto ref3 = cell.borrow();
        std::cout << "Final value: " << *ref3 << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
