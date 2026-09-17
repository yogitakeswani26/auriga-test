# Clinic Appointment System: Design Reasoning

## Purpose

This project is a console-based front-desk system for a clinic. Its first responsibility is to prevent scheduling conflicts. It also supports patient registration, appointment lookup, cancellation fees, receipts, rescheduling, doctor schedules, and a waitlist.

This document describes the design decisions and behavior of the implementation. It is a high-level engineering explanation, not a transcript of private internal reasoning.

## Core Rules

### 1. A doctor cannot be double-booked

Every appointment has:

- A doctor ID
- A start time
- A duration
- An end time calculated as `start + duration`

Two appointments conflict when:

```text
newStart < existingEnd && newEnd > existingStart
```

This uses half-open time intervals. Therefore:

- `09:00-09:30` and `09:30-10:00` are allowed
- `09:00-09:30` and `09:15-09:45` are rejected
- A conflict is checked only for the same doctor

The same overlap rule is also used when rescheduling.

### 2. A patient cannot have overlapping appointments

The system applies the same interval check to a patient's name. This prevents one patient from being booked with two doctors at the same time.

Patient names are compared case-insensitively for lookup and conflict checking.

### 3. Doctor working hours are enforced

Each doctor has opening and closing hours. The default clinic hours are 08:00 to 17:00.

An appointment is rejected when:

- It starts before the doctor's opening time
- It ends after the doctor's closing time

The doctor's consultation fee is stored with the doctor, so the front desk cannot accidentally charge a different fee during booking.

### 4. Cancellation policy

The default policy is:

- Cancellation at least 24 hours before the appointment: no fee
- Cancellation less than 24 hours before the appointment: Rs. 350 fee

The appointment is removed from the active schedule after cancellation, but a copy is kept in cancellation history so its receipt can still be generated.

## Main Data Objects

### Doctor

Stores the doctor's ID, name, working hours, and consultation fee.

### Patient

Stores the patient's name, phone number, age, gender, and medical history.

### Appointment

Stores the patient, doctor, time, duration, status, consultation fee, deposit, and medicines.

### Waitlist entry

Stores a patient's preferred doctor, preferred time, duration, and registration information when the requested slot is unavailable.

## Booking Flow

1. The front desk enters patient details.
2. The doctor list displays each doctor's consultation fee.
3. The front desk selects a doctor and time.
4. The system validates the doctor, duration, working hours, doctor conflicts, patient conflicts, fee, and deposit.
5. A valid appointment receives a unique ID.
6. A receipt is immediately printed and saved as `receipt_<id>.txt`.
7. If booking fails, the front desk can add the patient to the waitlist.

## Receipt Flow

Receipts can be generated:

- Automatically after a successful booking
- From menu option 5 using an appointment ID
- After a reschedule
- After a status update
- After cancellation, including cancelled appointments from history
- After viewing a doctor's day or week
- After finding a patient's appointments

A receipt contains:

- Appointment ID
- Patient details
- Doctor
- Date and time
- Duration
- Appointment status
- Consultation fee
- Deposit paid
- Balance due
- Medicines

The same formatted receipt is written to the terminal and to a text file so it can be opened, shared, or printed by the patient.

## Menu Design

The interactive menu is intentionally centered around front-desk workflows:

1. Book appointment
2. View doctor's day
3. Find patient's appointments
4. Cancel appointment
5. Generate patient receipt
6. Reschedule appointment
7. Find appointment by ID / update status
8. View doctor's week
9. Add patient to waitlist
10. Exit

The `--test` command-line argument runs the automated assertion suite without entering the interactive menu.

## Testing Strategy

The test suite validates the rules that can cause real scheduling problems:

- Overlapping appointments are rejected
- Back-to-back appointments are accepted
- Unknown doctors are rejected
- Invalid durations are rejected
- Appointments outside working hours are rejected
- A patient cannot overlap their own appointments
- Doctor-day results are sorted by time
- Patient lookup is case-insensitive
- Rescheduling respects conflicts and working hours
- Appointment status can be updated
- Waitlist entries are stored
- Early cancellation is free
- Late cancellation applies the configured fee
- Cancelled appointment receipts remain available
- Consultation fee, deposit, and medicines appear in stored appointment data

Build and test command:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -g main.cpp -o main
./main --test
```

## Important Implementation Choices

### Why use an in-memory vector?

The current project is a focused console prototype. Vectors keep the implementation easy to understand and make the scheduling rules visible. A database can be added later without changing the conflict rule itself.

### Why store money as cents?

Fees and deposits are stored as integers in cents rather than floating-point values. This avoids rounding errors when calculating balances.

### Why keep cancelled appointments separately?

Active scheduling should not include cancelled appointments, but patients may still need proof of the original booking and payment. Keeping cancellation history supports that receipt workflow.

### Why use a text receipt first?

A text receipt works in the current console environment, is easy to inspect, and does not require an external PDF library. PDF or email delivery can be added later.

## Known Limitations and Next Improvements

- Data is currently stored only in memory while the program runs.
- There are two sample doctors configured in `runMenu()`.
- Receipt files are text files, not PDF files.
- Patient lookup currently uses the patient's name.
- There is no login or role-based access control yet.
- Appointment data should eventually move to SQLite or another persistent database.
- A production system should use a timezone-aware date/time library and stronger input validation.
