#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <stdexcept> // власні виключення наслідують від std::runtime_error

// ===========================
// Власні виключення (п.9)
// ===========================
struct FileSaveError : public std::runtime_error { using std::runtime_error::runtime_error; };
struct EmptyClinicError : public std::runtime_error { using std::runtime_error::runtime_error; };
struct PatientIndexError : public std::runtime_error { using std::runtime_error::runtime_error; };

// ===========================
// БАЗОВА МОДЕЛЬ: Patient (батьківський клас)
// п.6: поліморфізм (віртуальний деструктор, віртуальні методи)
// п.8: поліморфне збереження (віртуальний toLine())
// ===========================
class Patient {
private:
    std::string name;
    int age{};
    std::string disease;

public:
    // Конструктори
    Patient() : name("Невідомо"), age(0), disease("Немає") {}
    Patient(std::string name, int age, std::string disease)
        : name(std::move(name)), age(age), disease(std::move(disease)) {
    }
    Patient(const Patient& other)
        : name(other.name), age(other.age), disease(other.disease) {
    }

    // Віртуальний деструктор (потрібен для коректного поліморфного видалення)
    virtual ~Patient() = default;

    // Поліморфний вивід у консоль
    virtual void printInfo() const {
        std::cout << "Пацієнт: " << name
            << ", вік: " << age
            << ", діагноз: " << disease << "\n";
    }

    // Поліморфне клонування (для глибокого копіювання у Polyclinic)
    virtual std::unique_ptr<Patient> clone() const {
        return std::make_unique<Patient>(*this);
    }

    // п.8: поліморфне представлення у вигляді одного рядка для файлу
    // Формат: TYPE|name|age|disease|...
    // Примітка: службовий токен TYPE залишаємо англійською (Patient/Child/Elder)
    virtual std::string toLine() const {
        return std::string("Patient") + "|" + name + "|" + std::to_string(age) + "|" + disease;
    }

    // Доступ до полів
    const std::string& getName() const { return name; }
    int getAge() const { return age; }
    const std::string& getDisease() const { return disease; }

    void setName(const std::string& n) { name = n; }
    void setAge(int a) { age = a; }
    void setDisease(const std::string& d) { disease = d; }

    // Порівняння (для повноти; не критично)
    bool operator==(const Patient& other) const { return name == other.name && age == other.age; }
    bool operator!=(const Patient& other) const { return !(*this == other); }
};

// ===========================
// ПОХІДНИЙ 1: ChildPatient
// Додаткове поле: контакт батьків; допоміжний метод: потреба дозволу батьків
// п.8: власна реалізація toLine()
// ===========================
class ChildPatient : public Patient {
private:
    std::string parentContact;

public:
    ChildPatient() : Patient(), parentContact("Немає контакту батьків") {}
    ChildPatient(std::string name, int age, std::string disease, std::string parentContact)
        : Patient(std::move(name), age, std::move(disease)),
        parentContact(std::move(parentContact)) {
    }

    std::unique_ptr<Patient> clone() const override {
        return std::make_unique<ChildPatient>(*this);
    }

    bool needParentalPermission() const {
        // Спрощено: якщо вік < 18 — потрібен дозвіл батьків
        return getAge() < 18;
    }

    void printInfo() const override {
        std::cout << "Дитячий пацієнт: " << getName()
            << ", вік: " << getAge()
            << ", діагноз: " << getDisease()
            << ", контакт батьків: " << parentContact
            << ", потрібен дозвіл: " << (needParentalPermission() ? "так" : "ні")
            << "\n";
    }

    std::string toLine() const override {
        return std::string("Child") + "|" + getName() + "|" + std::to_string(getAge())
            + "|" + getDisease() + "|" + parentContact;
    }
};

// ===========================
// ПОХІДНИЙ 2: ElderPatient
// Додаткові поля: алергії, протипоказання
// п.8: власна реалізація toLine()
// ===========================
class ElderPatient : public Patient {
private:
    std::string allergies;
    std::string contraindications;

public:
    ElderPatient() : Patient(), allergies("Немає"), contraindications("Немає") {}
    ElderPatient(std::string name, int age, std::string disease,
        std::string allergies, std::string contraindications)
        : Patient(std::move(name), age, std::move(disease)),
        allergies(std::move(allergies)),
        contraindications(std::move(contraindications)) {
    }

    std::unique_ptr<Patient> clone() const override {
        return std::make_unique<ElderPatient>(*this);
    }

    void printMedicalWarnings() const {
        std::cout << "  Алергії: " << allergies
            << " | Протипоказання: " << contraindications << "\n";
    }

    void printInfo() const override {
        std::cout << "Літній пацієнт: " << getName()
            << ", вік: " << getAge()
            << ", діагноз: " << getDisease() << "\n";
        printMedicalWarnings();
    }

    std::string toLine() const override {
        return std::string("Elder") + "|" + getName() + "|" + std::to_string(getAge())
            + "|" + getDisease() + "|" + allergies + "|" + contraindications;
    }
};

// ===========================
// Polyclinic: зберігає ПОЛІМОРФНИХ пацієнтів
// (std::unique_ptr<Patient>) + глибоке копіювання через clone()
// п.8: saveToFile() — «1 пацієнт = 1 рядок»
// п.9: кидання виключень у помилкових ситуаціях
// ===========================
class Polyclinic {
private:
    std::string name;
    std::string address;
    int doctorsCount{};
    std::vector<std::unique_ptr<Patient>> patients; // гетерогенний список (різні підтипи)

public:
    // Конструктори
    Polyclinic() : name("Без назви"), address("Невідомо"), doctorsCount(0) {}

    Polyclinic(std::string name, std::string address, int doctors)
        : name(std::move(name)), address(std::move(address)),
        doctorsCount(doctors < 0 ? 0 : doctors) {
    }

    // Глибоке копіювання: клонування кожного пацієнта
    Polyclinic(const Polyclinic& other)
        : name(other.name), address(other.address), doctorsCount(other.doctorsCount) {
        patients.reserve(other.patients.size());
        for (const auto& p : other.patients) patients.push_back(p->clone());
    }

    ~Polyclinic() = default; // unique_ptr автоматично звільнить пам'ять

    void printInfo() const {
        std::cout << "Поліклініка '" << name << "' за адресою " << address
            << " | лікарів: " << doctorsCount
            << " | пацієнтів: " << getPatientsCount() << "\n";
    }

    void printAllPatients() const {
        if (patients.empty()) {
            std::cout << "  [пацієнтів немає]\n";
            return;
        }
        for (const auto& p : patients) p->printInfo(); // поліморфний виклик
    }

    // Додавання / видалення пацієнтів
    void addPatient(const Patient& p) { patients.push_back(p.clone()); }

    void addChild(const std::string& pname, int age, const std::string& disease,
        const std::string& parentContact) {
        ChildPatient c{ pname, age, disease, parentContact };
        addPatient(c);
    }

    void addElder(const std::string& pname, int age, const std::string& disease,
        const std::string& allergies, const std::string& contraindications) {
        ElderPatient e{ pname, age, disease, allergies, contraindications };
        addPatient(e);
    }

    // п.9: кидати виключення при видаленні з порожньої клініки
    void removeLastPatient() {
        if (patients.empty()) throw EmptyClinicError("Немає пацієнтів для видалення");
        patients.pop_back();
    }

    // п.9: кидати виключення при неправильному індексі
    void removePatientByIndex(size_t index) {
        if (index >= patients.size()) throw PatientIndexError("Індекс за межами діапазону");
        patients.erase(patients.begin() + static_cast<std::ptrdiff_t>(index));
    }

    int getPatientsCount() const { return static_cast<int>(patients.size()); }

    const Patient* getPatientPtr(size_t index) const {
        if (index < patients.size()) return patients[index].get();
        return nullptr;
    }

    // Оператори (логіка як раніше; видалення може кинути EmptyClinicError)
    Polyclinic& operator++() { addPatient(Patient{}); return *this; }
    Polyclinic operator++(int) { Polyclinic t(*this); ++(*this); return t; }
    Polyclinic& operator--() { removeLastPatient(); return *this; }
    Polyclinic operator--(int) { Polyclinic t(*this); --(*this); return t; }

    Polyclinic operator+(const Polyclinic& other) const {
        Polyclinic merged(*this);
        merged.name = this->name + " + " + other.name;
        merged.doctorsCount = this->doctorsCount + other.doctorsCount;
        merged.patients.reserve(this->patients.size() + other.patients.size());
        for (const auto& p : other.patients) merged.patients.push_back(p->clone());
        return merged;
    }

    Polyclinic& operator+=(const Polyclinic& other) {
        this->name = this->name + " + " + other.name;
        this->doctorsCount += other.doctorsCount;
        patients.reserve(patients.size() + other.patients.size());
        for (const auto& p : other.patients) patients.push_back(p->clone());
        return *this;
    }

    bool operator==(const Polyclinic& other) const {
        return getPatientsCount() == other.getPatientsCount();
    }
    bool operator!=(const Polyclinic& other) const { return !(*this == other); }

    // п.8 + п.9: збереження у файл (кидає FileSaveError при невдачі)
    void saveToFile(const std::string& filepath) const {
        std::ofstream ofs(filepath);
        if (!ofs) throw FileSaveError("Не вдається відкрити файл: " + filepath);
        for (const auto& p : patients) ofs << p->toLine() << '\n'; // поліморфний виклик
    }
};

// ===========================
// Ролі та множинне успадкування (п.7)
// ===========================
class RoleUser {
public:
    // Перегляд інформації
    void viewClinic(const Polyclinic& clinic) const { clinic.printInfo(); }
    void viewPatients(const Polyclinic& clinic) const { clinic.printAllPatients(); }
};

class RoleAdmin {
public:
    // Адміністративні дії
    void addDefaultPatient(Polyclinic& clinic) const { clinic.addPatient(Patient{}); }
    void addChildPatient(Polyclinic& clinic,
        const std::string& name, int age,
        const std::string& disease, const std::string& parentContact) const {
        clinic.addChild(name, age, disease, parentContact);
    }
    void addElderPatient(Polyclinic& clinic,
        const std::string& name, int age,
        const std::string& disease,
        const std::string& allergies, const std::string& contraindications) const {
        clinic.addElder(name, age, disease, allergies, contraindications);
    }
    // п.9: при неправильному індексі проброситься PatientIndexError
    void removeAt(Polyclinic& clinic, size_t index) const {
        clinic.removePatientByIndex(index);
    }
};

class Manager : public RoleUser, public RoleAdmin {};

// ===========================
// Тести (п.6–9)
// ===========================
int main() {
    std::cout << "=== (пункти 1-6) ===\n";
    Polyclinic c1("Міська поліклініка №1", "вул. Головна, 10", 25);

    // Додаємо різні типи пацієнтів — зберігаються поліморфно
    c1.addChild("Марта", 7, "Застуда", "Мама: +380501112233");
    c1.addElder("Петро", 72, "Серцеве захворювання", "Пеніцилін", "Інтенсивні фізичні навантаження");
    c1.addPatient(Patient{ "Олексій", 40, "Грип" });

    c1.printInfo();
    c1.printAllPatients();

    std::cout << "\n=== (7) Ролі та множинне успадкування ===\n";
    RoleUser user;
    RoleAdmin admin;
    Manager manager;

    std::cout << "\n[Користувач МОЖЕ ПЕРЕГЛЯДАТИ]\n";
    user.viewClinic(c1);
    user.viewPatients(c1);

    std::cout << "\n[Адміністратор МОЖЕ ЗМІНЮВАТИ]\n";
    admin.addChildPatient(c1, "Олег", 12, "Травма", "Тато: +380631234567");
    admin.addElderPatient(c1, "Ірина", 67, "Діабет", "Немає", "Високовуглеводна дієта");
    admin.removeAt(c1, 0);
    admin.addDefaultPatient(c1);
    user.viewClinic(c1);
    user.viewPatients(c1);

    std::cout << "\n[Менеджер МОЖЕ І ПЕРЕГЛЯДАТИ, І ЗМІНЮВАТИ]\n";
    manager.viewClinic(c1);
    manager.addChildPatient(c1, "Андрій", 15, "Розтягнення зв'язок", "Мама: +380671112233");
    manager.viewPatients(c1);

    // ===========================
    // (8) Збереження у файл (поліморфний toLine)
    // ===========================
    std::cout << "\n=== (8) Збереження у файл (1 рядок на пацієнта) ===\n";
    try {
        c1.saveToFile("patients.txt");
        std::cout << "Збережено у 'patients.txt' → OK\n";
    }
    catch (const FileSaveError& e) {
        std::cout << "Помилка збереження: " << e.what() << "\n";
    }

    // ===========================
    // (9) Демонстрація обробки виключень
    // ===========================
    std::cout << "\n=== (9) Демонстрація виключень ===\n";

    // 1) PatientIndexError: спроба видалити за некоректним індексом
    try {
        std::cout << "[Тест] removeAt(c1, 999)\n";
        admin.removeAt(c1, 999);
    }
    catch (const PatientIndexError& e) {
        std::cout << "Спіймано PatientIndexError: " << e.what() << "\n";
    }

    // 2) EmptyClinicError: порожня поліклініка + спроба видалити
    Polyclinic empty("Порожня поліклініка", "Невідома адреса", 0);
    try {
        std::cout << "[Тест] --empty (видалення з порожньої поліклініки)\n";
        --empty; // викличе removeLastPatient() → кине EmptyClinicError
    }
    catch (const EmptyClinicError& e) {
        std::cout << "Спіймано EmptyClinicError: " << e.what() << "\n";
    }

    // 3) FileSaveError: спроба зберегти в неіснуючу директорію
    try {
        std::cout << "[Тест] збереження у 'nonexistent_dir/patients.txt'\n";
        c1.saveToFile("nonexistent_dir/patients.txt");
        std::cout << "Неочікувано: збереження успішне\n";
    }
    catch (const FileSaveError& e) {
        std::cout << "Спіймано FileSaveError: " << e.what() << "\n";
    }

    return 0;
}