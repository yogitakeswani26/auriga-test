# AI-LOGIC.md

## Scope

This file records the project-level conversation and implementation history for the clinic front-desk system. It contains the requested features, engineering decisions, fixes, validations, and delivery steps. It does not contain private system instructions, hidden reasoning, credentials, or a verbatim chat transcript.

## Project Goal

Build a clinic front-desk application that:

- Prevents doctors from being double-booked
- Prevents patients from having overlapping appointments
- Supports timely and late cancellation rules
- Lets staff view doctor schedules
- Finds appointments by patient name or appointment ID
- Generates complete patient receipts

## Conversation And Implementation Log

### 1. Initial clinic requirement

The initial requirement described a busy clinic where:

- Front-desk staff book patients into time slots
- Doctors must never have overlapping appointments
- Patients can cancel appointments
- Cancellations within 24 hours receive a small fee
- Staff need doctor-day views and patient lookups

The implementation was built as a C++ console application in `main.cpp`.

### 2. Core scheduling implementation

The first version introduced:

- `Doctor`, `Patient`, and `Appointment` data structures
- A `Clinic` class
- Doctor conflict detection
- Doctor-day schedule lookup
- Patient appointment lookup
- Cancellation with a Rs. 350 late fee
- A simple demonstration flow

The conflict rule uses time intervals:

```text
newStart < existingEnd && newEnd > existingStart
```

This allows back-to-back appointments but rejects actual overlaps.

### 3. Automated tests

Assertion-based tests were added and made available through:

```bash
./main --test
```

The tests cover:

- Overlapping appointment rejection
- Back-to-back appointment acceptance
- Invalid doctor rejection
- Invalid duration rejection
- Doctor-day sorting
- Case-insensitive patient lookup
- Free cancellation
- Late cancellation fee
- Missing appointment cancellation

### 4. Interactive front desk menu

The fixed demonstration was replaced by an interactive menu. The front desk can now:

1. Book an appointment
2. View a doctor's day
3. Find a patient's appointments
4. Cancel an appointment
5. Generate a patient receipt
6. Reschedule an appointment
7. Find an appointment by ID and update its status
8. View a doctor's week
9. Add a patient to the waitlist
10. Exit

The application also handles closed terminal input without getting stuck in an input-validation loop.

### 5. Patient registration

Patient details were expanded to include:

- Name
- Phone number
- Age
- Gender
- Medical history

Patients are registered or updated when an appointment is booked.

### 6. Higher-priority clinic features

The following features were added:

- Doctor working hours, defaulting to 08:00-17:00
- Rescheduling with conflict and working-hour validation
- Appointment statuses such as `Booked`, `Checked-in`, and `Cancelled`
- Duplicate patient overlap prevention
- Appointment ID lookup
- Weekly doctor schedule display
- Waitlist entries for unavailable slots

When a requested booking fails, the front desk can add the patient to the waitlist.

### 7. Doctor-specific consultation fees

Consultation fees were moved to doctor profiles:

- Dr. Anika Patel: Rs. 800
- Dr. Marcus Chen: Rs. 1000

The selected doctor's fee is shown beside the doctor's name. Staff only enter the deposit during booking; the consultation fee is automatically applied to the appointment and receipt.

### 8. Patient receipt feature

Receipts include:

- Appointment ID
- Patient name
- Phone number
- Age and gender
- Medical history
- Doctor name
- Appointment date and time
- Duration
- Appointment status
- Consultation fee
- Deposit paid
- Balance due
- Medicines

Receipts are generated automatically after successful booking and can also be generated later using the appointment ID.

Receipts are saved as text files using this format:

```text
receipt_<appointment-id>.txt
```

Cancelled appointment history is retained so a cancelled appointment's receipt can still be generated.

Waitlist entries generate a separate waitlist receipt stating that no consultation fee has been charged yet.

### 9. Receipt menu correction

During testing, menu numbering was clarified so option `5` directly generates a receipt. Rescheduling and the remaining actions were shifted to later menu options.

The booking flow was then changed so a receipt is printed automatically immediately after a successful booking. No extra `y/n` confirmation is required.

### 10. Debugging fix

Compiler diagnostics for `main.cpp` were checked and confirmed clean. A missing `.vscode/launch.json` was identified as a likely cause of debugger-console problems.

A debugger configuration was added with:

- `cppdbg`
- GDB
- Integrated terminal
- The workspace `main` executable
- The existing C++ build task as a pre-launch task

### 11. Validation history

The following validation command has been used repeatedly:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -g main.cpp -o main
./main --test
```

The final test output was:

```text
All tests passed.
```

A complete interactive booking flow was also tested with patient details, doctor selection, deposit, medicines, and automatic receipt generation.

## Repository Delivery

The project was pushed to:

```text
https://github.com/yogitakeswani26/auriga-test
```

Relevant commits:

- `fd753af` - Add clinic appointment and receipt system
- `ca422e7` - Document clinic system design decisions

This file is being added as a project conversation and implementation log.

## Current Limitations

- Appointments are stored in memory while the program runs.
- A database is not yet connected.
- Receipts are text files rather than PDFs.
- The sample menu configures two doctors.
- Authentication and role-based permissions are not implemented.
- A production deployment should use timezone-aware date/time handling.

## Suggested Next Features

- SQLite persistence
- PDF receipt generation
- Email or SMS receipt delivery
- Login for front desk, doctors, and administrators
- Doctor and clinic configuration screens
- Payment transaction history
- Automatic waitlist promotion after cancellation
- CSV and PDF reports
