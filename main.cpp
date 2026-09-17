#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

struct Doctor {
    int id;
    std::string name;
    int openingHour = 8;
    int closingHour = 17;
    int consultationFeeCents = 0;
};

struct Patient {
    std::string name;
    std::string phone;
    int age = 0;
    std::string gender;
    std::string medicalHistory;
};

struct Appointment {
    int id;
    Patient patient;
    int doctorId;
    std::time_t start;
    int durationMinutes;
    std::string status = "Booked";
    int consultationFeeCents = 0;
    int depositCents = 0;
    std::string medicines;
};

struct WaitlistEntry {
    Patient patient;
    int doctorId;
    std::time_t preferredStart;
    int durationMinutes;
};

struct CancellationResult {
    bool cancelled;
    bool late;
    int feeCents;
    std::string message;
};

class Clinic {
public:
    explicit Clinic(int lateCancellationHours = 24, int lateFeeCents = 3500)
        : lateCancellationHours_(lateCancellationHours), lateFeeCents_(lateFeeCents) {}

    void addDoctor(int id, const std::string& name, int openingHour = 8,
                   int closingHour = 17, int consultationFeeCents = 0) {
        doctors_.push_back({id, name, openingHour, closingHour, consultationFeeCents});
    }

    bool registerPatient(const Patient& patient, std::string& error) {
        if (patient.name.empty()) {
            error = "Patient name is required.";
            return false;
        }
        auto existing = std::find_if(patients_.begin(), patients_.end(),
                                     [&](const Patient& saved) {
                                         return equalsIgnoreCase(saved.name, patient.name);
                                     });
        if (existing == patients_.end()) {
            patients_.push_back(patient);
        } else {
            *existing = patient;
        }
        return true;
    }

    bool bookAppointment(const std::string& patientName,
                         int doctorId,
                         std::time_t start,
                         int durationMinutes,
                         std::string& error) {
        return bookAppointment(Patient{patientName, "", 0, "", ""},
                       doctorId, start, durationMinutes, error);
    }

    bool bookAppointment(const Patient& patient,
                         int doctorId,
                         std::time_t start,
                         int durationMinutes,
                         std::string& error) {
        return bookAppointment(patient, doctorId, start, durationMinutes,
                       0, 0, "", error);
        }

        bool bookAppointment(const Patient& patient,
                 int doctorId,
                 std::time_t start,
                 int durationMinutes,
                 int consultationFeeCents,
                 int depositCents,
                 const std::string& medicines,
                 std::string& error) {
        if (patient.name.empty()) {
            error = "Patient name is required.";
            return false;
        }
        if (!doctorExists(doctorId)) {
            error = "Doctor does not exist.";
            return false;
        }
        const Doctor* doctor = getDoctor(doctorId);
        const int startMinutes = localMinutes(start);
        const int endMinutes = startMinutes + durationMinutes;
        if (startMinutes < doctor->openingHour * 60 || endMinutes > doctor->closingHour * 60) {
            error = "Appointment is outside the doctor's working hours.";
            return false;
        }
        if (hasPatientConflict(patient.name, start, durationMinutes)) {
            error = "This patient already has an overlapping appointment.";
            return false;
        }
        if (durationMinutes <= 0) {
            error = "Duration must be positive.";
            return false;
        }
        if (consultationFeeCents < 0 || depositCents < 0 || depositCents > consultationFeeCents) {
            error = "Deposit must be non-negative and cannot exceed the consultation fee.";
            return false;
        }
        if (hasConflict(doctorId, start, durationMinutes)) {
            error = "The doctor already has an overlapping appointment.";
            return false;
        }

        appointments_.push_back({nextAppointmentId_++, patient, doctorId, start, durationMinutes,
                      "Booked", consultationFeeCents, depositCents, medicines});
        registerPatient(patient, error);
        return true;
    }

    bool rescheduleAppointment(int appointmentId, std::time_t newStart,
                               int newDuration, std::string& error) {
        auto found = findAppointment(appointmentId);
        if (found == appointments_.end()) {
            error = "Appointment not found.";
            return false;
        }
        const Doctor* doctor = getDoctor(found->doctorId);
        const int startMinutes = localMinutes(newStart);
        if (startMinutes < doctor->openingHour * 60 ||
            startMinutes + newDuration > doctor->closingHour * 60) {
            error = "New time is outside the doctor's working hours.";
            return false;
        }
        if (hasConflict(found->doctorId, newStart, newDuration, appointmentId) ||
            hasPatientConflict(found->patient.name, newStart, newDuration, appointmentId)) {
            error = "The new time overlaps another appointment.";
            return false;
        }
        found->start = newStart;
        found->durationMinutes = newDuration;
        return true;
    }

    std::vector<Appointment> doctorDay(int doctorId, std::time_t day) const {
        std::vector<Appointment> result;
        for (const Appointment& appointment : appointments_) {
            if (appointment.doctorId == doctorId && sameDay(appointment.start, day)) {
                result.push_back(appointment);
            }
        }
        std::sort(result.begin(), result.end(), [](const Appointment& left, const Appointment& right) {
            return left.start < right.start;
        });
        return result;
    }

    std::vector<Appointment> findAppointmentsByPatient(const std::string& patientName) const {
        std::vector<Appointment> result;
        for (const Appointment& appointment : appointments_) {
            if (equalsIgnoreCase(appointment.patient.name, patientName)) {
                result.push_back(appointment);
            }
        }
        std::sort(result.begin(), result.end(), [](const Appointment& left, const Appointment& right) {
            return left.start < right.start;
        });
        return result;
    }

    const Appointment* findAppointmentById(int appointmentId) const {
        const auto found = std::find_if(appointments_.begin(), appointments_.end(),
                                        [appointmentId](const Appointment& appointment) {
                                            return appointment.id == appointmentId;
                                        });
        return found == appointments_.end() ? nullptr : &*found;
    }

    const Appointment* findReceiptById(int appointmentId) const {
        const Appointment* active = findAppointmentById(appointmentId);
        if (active) return active;
        const auto found = std::find_if(cancelledAppointments_.begin(), cancelledAppointments_.end(),
                                        [appointmentId](const Appointment& appointment) {
                                            return appointment.id == appointmentId;
                                        });
        return found == cancelledAppointments_.end() ? nullptr : &*found;
    }

    bool updateStatus(int appointmentId, const std::string& status) {
        auto found = findAppointment(appointmentId);
        if (found == appointments_.end()) return false;
        found->status = status;
        return true;
    }

    void addToWaitlist(const Patient& patient, int doctorId,
                       std::time_t preferredStart, int durationMinutes) {
        waitlist_.push_back({patient, doctorId, preferredStart, durationMinutes});
    }

    const std::vector<WaitlistEntry>& waitlist() const { return waitlist_; }

    CancellationResult cancelAppointment(int appointmentId, std::time_t cancelledAt) {
        const auto found = std::find_if(appointments_.begin(), appointments_.end(),
                                        [appointmentId](const Appointment& appointment) {
                                            return appointment.id == appointmentId;
                                        });
        if (found == appointments_.end()) {
            return {false, false, 0, "Appointment not found."};
        }

        const auto hoursUntilAppointment = std::chrono::duration_cast<std::chrono::hours>(
            std::chrono::system_clock::from_time_t(found->start) -
            std::chrono::system_clock::from_time_t(cancelledAt));
        const bool late = hoursUntilAppointment.count() < lateCancellationHours_;
        const int fee = late ? lateFeeCents_ : 0;
        found->status = "Cancelled";
        cancelledAppointments_.push_back(*found);
        appointments_.erase(found);

        return {true, late, fee, late ? "Cancelled with a late-cancellation fee."
                                      : "Cancelled without a fee."};
    }

    const Doctor* getDoctor(int doctorId) const {
        const auto found = std::find_if(doctors_.begin(), doctors_.end(),
                                       [doctorId](const Doctor& doctor) {
                                           return doctor.id == doctorId;
                                       });
        return found == doctors_.end() ? nullptr : &*found;
    }

    int consultationFeeCents(int doctorId) const {
        const Doctor* doctor = getDoctor(doctorId);
        return doctor ? doctor->consultationFeeCents : 0;
    }

private:
    std::vector<Appointment>::iterator findAppointment(int appointmentId) {
        return std::find_if(appointments_.begin(), appointments_.end(),
                            [appointmentId](const Appointment& appointment) {
                                return appointment.id == appointmentId;
                            });
    }

    bool doctorExists(int doctorId) const {
        return getDoctor(doctorId) != nullptr;
    }

    bool hasConflict(int doctorId, std::time_t start, int durationMinutes,
                     int ignoredAppointmentId = -1) const {
        const std::time_t end = start + durationMinutes * 60;
        for (const Appointment& appointment : appointments_) {
            if (appointment.doctorId != doctorId || appointment.id == ignoredAppointmentId) {
                continue;
            }
            const std::time_t appointmentEnd = appointment.start + appointment.durationMinutes * 60;
            if (start < appointmentEnd && end > appointment.start) {
                return true;
            }
        }
        return false;
    }

    bool hasPatientConflict(const std::string& patientName, std::time_t start,
                            int durationMinutes, int ignoredAppointmentId = -1) const {
        const std::time_t end = start + durationMinutes * 60;
        for (const Appointment& appointment : appointments_) {
            if (appointment.id == ignoredAppointmentId ||
                !equalsIgnoreCase(appointment.patient.name, patientName)) continue;
            const std::time_t appointmentEnd = appointment.start + appointment.durationMinutes * 60;
            if (start < appointmentEnd && end > appointment.start) return true;
        }
        return false;
    }

    static int localMinutes(std::time_t value) {
        std::tm local = *std::localtime(&value);
        return local.tm_hour * 60 + local.tm_min;
    }

    static bool sameDay(std::time_t left, std::time_t right) {
        std::tm leftDate = *std::localtime(&left);
        std::tm rightDate = *std::localtime(&right);
        return leftDate.tm_year == rightDate.tm_year &&
               leftDate.tm_mon == rightDate.tm_mon &&
               leftDate.tm_mday == rightDate.tm_mday;
    }

    static bool equalsIgnoreCase(std::string left, std::string right) {
        std::transform(left.begin(), left.end(), left.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        std::transform(right.begin(), right.end(), right.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return left == right;
    }

    std::vector<Doctor> doctors_;
    std::vector<Patient> patients_;
    std::vector<Appointment> appointments_;
    std::vector<Appointment> cancelledAppointments_;
    std::vector<WaitlistEntry> waitlist_;
    int nextAppointmentId_ = 1;
    int lateCancellationHours_;
    int lateFeeCents_;
};

std::time_t makeTime(int year, int month, int day, int hour, int minute) {
    std::tm value{};
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_isdst = -1;
    return std::mktime(&value);
}

std::string formatTime(std::time_t value) {
    std::tm local = *std::localtime(&value);
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M");
    return output.str();
}

void printAppointments(const Clinic& clinic, const std::vector<Appointment>& appointments) {
    for (const Appointment& appointment : appointments) {
        const Doctor* doctor = clinic.getDoctor(appointment.doctorId);
        std::cout << "#" << appointment.id << " " << appointment.patient.name
                  << " | " << (doctor ? doctor->name : "Unknown doctor")
                  << " | " << formatTime(appointment.start)
                  << " | " << appointment.durationMinutes << " minutes\n";
    }
}

void runTests() {
    const std::time_t day = makeTime(2026, 9, 18, 0, 0);
    const std::time_t nineAM = makeTime(2026, 9, 18, 9, 0);
    std::string error;

    {
        Clinic clinic;
        clinic.addDoctor(1, "Dr. Patel");
        clinic.addDoctor(2, "Dr. Chen");

        assert(clinic.bookAppointment("First Patient", 1, nineAM, 30, error));
        assert(!clinic.bookAppointment("Overlapping Patient", 1,
                                       makeTime(2026, 9, 18, 9, 15), 30, error));
        assert(error == "The doctor already has an overlapping appointment.");

        // An appointment ending at 09:30 may be followed by one starting at 09:30.
        assert(clinic.bookAppointment("Next Patient", 1,
                                      makeTime(2026, 9, 18, 9, 30), 30, error));

        const Patient registeredPatient{
            "Registered Patient", "+91 9876543210", 34, "Female", "Asthma"};
        assert(clinic.bookAppointment(registeredPatient, 1,
                                      makeTime(2026, 9, 18, 10, 0), 30,
                                      7500, 2500, "Paracetamol 500mg", error));
        const std::vector<Appointment> registered =
            clinic.findAppointmentsByPatient("Registered Patient");
        assert(registered.size() == 1);
        assert(registered[0].patient.phone == "+91 9876543210");
        assert(registered[0].patient.age == 34);
        assert(registered[0].patient.medicalHistory == "Asthma");
        assert(registered[0].consultationFeeCents == 7500);
        assert(registered[0].depositCents == 2500);
        assert(registered[0].medicines == "Paracetamol 500mg");
        assert(!clinic.bookAppointment("Outside Hours", 1,
                           makeTime(2026, 9, 18, 17, 0), 30, error));
        assert(error == "Appointment is outside the doctor's working hours.");
        assert(!clinic.bookAppointment("Registered Patient", 2,
                           makeTime(2026, 9, 18, 10, 15), 30, error));
        assert(error == "This patient already has an overlapping appointment.");
    }

    {
        Clinic clinic;
        clinic.addDoctor(1, "Dr. Patel");

        assert(!clinic.bookAppointment("Unknown Doctor", 99, nineAM, 30, error));
        assert(error == "Doctor does not exist.");
        assert(!clinic.bookAppointment("Invalid Duration", 1, nineAM, 0, error));
        assert(error == "Duration must be positive.");
    }

    {
        Clinic clinic;
        clinic.addDoctor(1, "Dr. Patel");
        clinic.addDoctor(2, "Dr. Chen");

        assert(clinic.bookAppointment("Later Patient", 1,
                                      makeTime(2026, 9, 18, 11, 0), 30, error));
        assert(clinic.bookAppointment("Earlier Patient", 1, nineAM, 30, error));
        assert(clinic.bookAppointment("Other Doctor", 2, nineAM, 30, error));

        const std::vector<Appointment> appointments = clinic.doctorDay(1, day);
        assert(appointments.size() == 2);
        assert(appointments[0].patient.name == "Earlier Patient");
        assert(appointments[1].patient.name == "Later Patient");

        const std::vector<Appointment> matches =
            clinic.findAppointmentsByPatient("earlier patient");
        assert(matches.size() == 1);
        assert(matches[0].patient.name == "Earlier Patient");
        assert(clinic.findAppointmentsByPatient("Not Booked").empty());
        assert(clinic.rescheduleAppointment(1, makeTime(2026, 9, 18, 12, 0), 30, error));
        assert(clinic.findAppointmentById(1) != nullptr);
        assert(clinic.findAppointmentById(1)->start == makeTime(2026, 9, 18, 12, 0));
        assert(clinic.updateStatus(1, "Checked-in"));
        assert(clinic.findAppointmentById(1)->status == "Checked-in");
        clinic.addToWaitlist({"Waiting Patient", "123", 20, "Male", ""}, 1, nineAM, 30);
        assert(clinic.waitlist().size() == 1);
    }

    {
        Clinic clinic;
        clinic.addDoctor(1, "Dr. Patel");
        clinic.addDoctor(2, "Dr. Chen");
        assert(clinic.bookAppointment("Free Cancellation", 1, nineAM, 30, error));
        assert(clinic.bookAppointment("Late Cancellation", 2, nineAM, 30, error));

        const CancellationResult freeCancellation = clinic.cancelAppointment(
            1, makeTime(2026, 9, 17, 9, 0));
        assert(freeCancellation.cancelled);
        assert(!freeCancellation.late);
        assert(freeCancellation.feeCents == 0);

        const CancellationResult lateCancellation = clinic.cancelAppointment(
            2, makeTime(2026, 9, 17, 9, 1));
        assert(lateCancellation.cancelled);
        assert(lateCancellation.late);
        assert(lateCancellation.feeCents == 3500);
        assert(clinic.findReceiptById(2) != nullptr);
        assert(clinic.findReceiptById(2)->status == "Cancelled");

        const CancellationResult missingCancellation = clinic.cancelAppointment(
            999, makeTime(2026, 9, 17, 9, 0));
        assert(!missingCancellation.cancelled);
        assert(missingCancellation.feeCents == 0);
    }

    std::cout << "All tests passed.\n";
}

std::time_t readDateTime(const std::string& prompt) {
    while (true) {
        std::cout << prompt << " (YYYY-MM-DD HH:MM): ";
        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cout << "\nInput closed. Exiting.\n";
            std::exit(0);
        }
        std::tm value{};
        std::istringstream parser(input);
        parser >> std::get_time(&value, "%Y-%m-%d %H:%M");
        if (!parser.fail()) {
            value.tm_isdst = -1;
            return std::mktime(&value);
        }
        std::cout << "Invalid date/time format. Try again.\n";
    }
}

int readInteger(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cout << "\nInput closed. Exiting.\n";
            std::exit(0);
        }
        std::istringstream parser(input);
        int value;
        char extra;
        if ((parser >> value) && !(parser >> extra)) {
            return value;
        }
        std::cout << "Please enter a valid number.\n";
    }
}

std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string input;
    if (!std::getline(std::cin, input)) {
        std::cout << "\nInput closed. Exiting.\n";
        std::exit(0);
    }
    return input;
}

void printReceipt(const Clinic& clinic, const Appointment& appointment);

void offerReceipt(const Clinic& clinic, int appointmentId) {
    const std::string answer = readLine("Generate receipt now? (y/n): ");
    if (answer == "y" || answer == "Y") {
        const Appointment* appointment = clinic.findReceiptById(appointmentId);
        if (appointment) printReceipt(clinic, *appointment);
    }
}

void offerReceiptSelection(const Clinic& clinic) {
    const std::string input = readLine("Receipt ke liye appointment ID enter karein (skip ke liye blank): ");
    if (input.empty()) return;
    std::istringstream parser(input);
    int appointmentId;
    if (!(parser >> appointmentId)) {
        std::cout << "Invalid appointment ID.\n";
        return;
    }
    const Appointment* appointment = clinic.findReceiptById(appointmentId);
    if (appointment) printReceipt(clinic, *appointment);
    else std::cout << "Appointment not found.\n";
}

void printWaitlistReceipt(const Clinic& clinic, const WaitlistEntry& entry) {
    const Doctor* doctor = clinic.getDoctor(entry.doctorId);
    std::cout << "\n========================================\n"
              << "           WAITLIST RECEIPT\n"
              << "========================================\n"
              << "Patient: " << entry.patient.name << '\n'
              << "Phone: " << entry.patient.phone << '\n'
              << "Age / Gender: " << entry.patient.age << " / " << entry.patient.gender << '\n'
              << "Doctor: " << (doctor ? doctor->name : "Unknown doctor") << '\n'
              << "Preferred time: " << formatTime(entry.preferredStart) << '\n'
              << "Duration: " << entry.durationMinutes << " minutes\n"
              << "Status: Waiting list\n"
              << "No consultation fee has been charged yet.\n"
              << "========================================\n";
}

void printMoney(int cents);

void printDoctors(const Clinic& clinic) {
    const Doctor* firstDoctor = clinic.getDoctor(1);
    const Doctor* secondDoctor = clinic.getDoctor(2);
    std::cout << "Doctors:\n"
              << "1. " << firstDoctor->name << " (Fee: ";
    printMoney(firstDoctor->consultationFeeCents);
    std::cout << ")\n2. " << secondDoctor->name << " (Fee: ";
    printMoney(secondDoctor->consultationFeeCents);
    std::cout << ")\n";
}

void printAppointmentDetails(const Clinic& clinic, const Appointment& appointment) {
    printAppointments(clinic, {appointment});
    std::cout << "Status: " << appointment.status << '\n'
              << "Patient phone: " << appointment.patient.phone << '\n'
              << "Age: " << appointment.patient.age << " | Gender: " << appointment.patient.gender << '\n'
              << "Medical history: " << appointment.patient.medicalHistory << '\n';
}

void printMoney(int cents) {
    std::cout << "Rs. " << cents / 100 << '.' << std::setw(2) << std::setfill('0')
              << cents % 100 << std::setfill(' ');
}

void printMoneyTo(std::ostream& output, int cents) {
    output << "Rs. " << cents / 100 << '.' << std::setw(2) << std::setfill('0')
           << cents % 100 << std::setfill(' ');
}

void writeReceipt(std::ostream& output, const Clinic& clinic, const Appointment& appointment) {
    const Doctor* doctor = clinic.getDoctor(appointment.doctorId);
    const int balanceCents = appointment.consultationFeeCents - appointment.depositCents;

    output << "\n========================================\n"
           << "              CLINIC RECEIPT\n"
           << "========================================\n"
           << "Appointment ID: " << appointment.id << '\n'
           << "Patient: " << appointment.patient.name << '\n'
           << "Phone: " << appointment.patient.phone << '\n'
           << "Age / Gender: " << appointment.patient.age << " / "
           << appointment.patient.gender << '\n'
           << "Medical history: " << appointment.patient.medicalHistory << '\n'
           << "Doctor: " << (doctor ? doctor->name : "Unknown doctor") << '\n'
           << "Date and time: " << formatTime(appointment.start) << '\n'
           << "Duration: " << appointment.durationMinutes << " minutes\n"
           << "Status: " << appointment.status << '\n'
           << "----------------------------------------\n"
           << "Consultation fee: ";
    printMoneyTo(output, appointment.consultationFeeCents);
    output << "\nDeposit paid:      ";
    printMoneyTo(output, appointment.depositCents);
    output << "\nBalance due:       ";
    printMoneyTo(output, balanceCents);
    output << "\nMedicines: " << (appointment.medicines.empty() ? "None" : appointment.medicines)
           << "\n========================================\n";
}

void printReceipt(const Clinic& clinic, const Appointment& appointment) {
    writeReceipt(std::cout, clinic, appointment);
    const std::string filename = "receipt_" + std::to_string(appointment.id) + ".txt";
    std::ofstream receiptFile(filename);
    if (receiptFile) {
        writeReceipt(receiptFile, clinic, appointment);
        std::cout << "Receipt saved for patient: " << filename << '\n';
    } else {
        std::cout << "Receipt could not be saved to a file.\n";
    }
}

void runMenu() {
    Clinic clinic;
    clinic.addDoctor(1, "Dr. Anika Patel", 8, 17, 80000);
    clinic.addDoctor(2, "Dr. Marcus Chen", 8, 17, 100000);

    while (true) {
        std::cout << "\n=== Clinic Front Desk ===\n"
                  << "1. Book appointment\n"
                  << "2. View doctor's day\n"
                  << "3. Find patient's appointments\n"
                  << "4. Cancel appointment\n"
                  << "5. Generate patient receipt\n"
                  << "6. Reschedule appointment\n"
                  << "7. Find appointment by ID / update status\n"
                  << "8. View doctor's week\n"
                  << "9. Add patient to waitlist\n"
                  << "10. Exit\n";
        const int choice = readInteger("Choose an option: ");

        if (choice == 1) {
            Patient patient;
            patient.name = readLine("Patient name: ");
            patient.phone = readLine("Phone number: ");
            patient.age = readInteger("Age: ");
            patient.gender = readLine("Gender: ");
            patient.medicalHistory = readLine("Medical history (optional): ");
            printDoctors(clinic);
            const int doctorId = readInteger("Doctor number: ");
            const std::time_t start = readDateTime("Appointment start");
            const int duration = readInteger("Duration in minutes: ");
            const int deposit = readInteger("Deposit paid (in rupees): ");
            const std::string medicines = readLine("Medicines (optional): ");
            std::string error;
            const int consultationFee = clinic.consultationFeeCents(doctorId);
            if (clinic.bookAppointment(patient, doctorId, start, duration,
                                       consultationFee, deposit * 100,
                                       medicines, error)) {
                std::cout << "Appointment booked successfully.\n";
                const std::vector<Appointment> bookedAppointments =
                    clinic.findAppointmentsByPatient(patient.name);
                if (!bookedAppointments.empty()) {
                    printReceipt(clinic, bookedAppointments.back());
                }
            } else {
                std::cout << "Booking failed: " << error << '\n';
                const std::string addWaitlist = readLine("Add patient to waitlist? (y/n): ");
                if (addWaitlist == "y" || addWaitlist == "Y") {
                    clinic.addToWaitlist(patient, doctorId, start, duration);
                    std::cout << "Patient added to waitlist.\n";
                }
            }
        } else if (choice == 2) {
            printDoctors(clinic);
            const int doctorId = readInteger("Doctor number: ");
            const std::time_t day = readDateTime("Enter any time on the day");
            const std::vector<Appointment> appointments = clinic.doctorDay(doctorId, day);
            if (appointments.empty()) {
                std::cout << "No appointments found.\n";
            } else {
                printAppointments(clinic, appointments);
            }
            offerReceiptSelection(clinic);
        } else if (choice == 3) {
            std::string patientName;
            std::cout << "Patient name: ";
            std::getline(std::cin, patientName);
            const std::vector<Appointment> appointments =
                clinic.findAppointmentsByPatient(patientName);
            if (appointments.empty()) {
                std::cout << "No appointments found.\n";
            } else {
                printAppointments(clinic, appointments);
            }
            offerReceiptSelection(clinic);
        } else if (choice == 4) {
            const int appointmentId = readInteger("Appointment ID: ");
            const std::time_t cancelledAt = readDateTime("Cancellation time");
            const CancellationResult result = clinic.cancelAppointment(appointmentId, cancelledAt);
            std::cout << result.message;
            if (result.feeCents > 0) {
                std::cout << " Fee: $" << result.feeCents / 100 << '.'
                          << std::setw(2) << std::setfill('0') << result.feeCents % 100;
            }
            std::cout << '\n';
            if (result.cancelled) offerReceipt(clinic, appointmentId);
        } else if (choice == 5) {
            const int appointmentId = readInteger("Appointment ID for receipt: ");
            const Appointment* appointment = clinic.findReceiptById(appointmentId);
            if (!appointment) {
                std::cout << "Appointment not found. Please use a valid appointment ID.\n";
            } else {
                printReceipt(clinic, *appointment);
            }
        } else if (choice == 6) {
            const int appointmentId = readInteger("Appointment ID: ");
            const std::time_t newStart = readDateTime("New appointment time");
            const int duration = readInteger("New duration in minutes: ");
            std::string error;
            if (clinic.rescheduleAppointment(appointmentId, newStart, duration, error)) {
                std::cout << "Appointment rescheduled.\n";
                offerReceipt(clinic, appointmentId);
            } else {
                std::cout << "Reschedule failed: " << error << '\n';
            }
        } else if (choice == 7) {
            const int appointmentId = readInteger("Appointment ID: ");
            const Appointment* appointment = clinic.findAppointmentById(appointmentId);
            if (!appointment) {
                std::cout << "Appointment not found.\n";
            } else {
                printAppointmentDetails(clinic, *appointment);
                const std::string status = readLine(
                    "New status (leave blank to keep current): ");
                if (!status.empty() && clinic.updateStatus(appointmentId, status)) {
                    std::cout << "Status updated.\n";
                }
                offerReceipt(clinic, appointmentId);
            }
        } else if (choice == 8) {
            printDoctors(clinic);
            const int doctorId = readInteger("Doctor number: ");
            const std::time_t firstDay = readDateTime("Enter any time on the first day");
            for (int offset = 0; offset < 7; ++offset) {
                std::tm day = *std::localtime(&firstDay);
                day.tm_mday += offset;
                day.tm_hour = 0;
                day.tm_min = 0;
                const std::time_t date = std::mktime(&day);
                std::cout << "\n" << formatTime(date).substr(0, 10) << ":\n";
                const std::vector<Appointment> appointments = clinic.doctorDay(doctorId, date);
                if (appointments.empty()) std::cout << "No appointments.\n";
                else printAppointments(clinic, appointments);
            }
            offerReceiptSelection(clinic);
        } else if (choice == 9) {
            Patient patient;
            patient.name = readLine("Patient name: ");
            patient.phone = readLine("Phone number: ");
            patient.age = readInteger("Age: ");
            patient.gender = readLine("Gender: ");
            patient.medicalHistory = readLine("Medical history (optional): ");
            printDoctors(clinic);
            const int doctorId = readInteger("Doctor number: ");
            const std::time_t preferredStart = readDateTime("Preferred appointment time");
            const int duration = readInteger("Duration in minutes: ");
            clinic.addToWaitlist(patient, doctorId, preferredStart, duration);
            std::cout << "Patient added to waitlist.\n";
            printWaitlistReceipt(clinic, clinic.waitlist().back());
        } else if (choice == 10) {
            std::cout << "Goodbye.\n";
            return;
        } else {
            std::cout << "Invalid option.\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--test") {
        runTests();
        return 0;
    }
    runMenu();
}
