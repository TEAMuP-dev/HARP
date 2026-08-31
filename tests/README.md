# HARP Unit Testing Framework

## Overview

HARP serves as the application layer responsible for interpreting model interactions, maintaining application state, and coordinating communication between external model wrappers and the user-facing graphical interface. While integration testing verifies that these components communicate correctly, successful communication alone does not guarantee that HARP's internal logic is functioning correctly. Errors in parsing, state management, validation, or decision-making can still produce an incorrect user experience despite successful wrapper execution.

This testing framework provides a structured methodology for validating HARP-owned logic independently of wrapper communication. Rather than maximizing code coverage, the framework focuses on verifying the critical invariants that govern HARP's behavior. Each selected unit test is designed to provide meaningful evidence that a specific aspect of HARP's internal logic behaves correctly under both normal and edge-case conditions.

The framework complements, rather than replaces, integration testing. Together, unit tests and integration tests provide confidence that both HARP's internal logic and its communication with external components operate as intended.

## Why This Framework Exists

Because HARP serves as an intermediary application layer between external wrappers and the graphical interface, no single testing strategy can completely validate its correctness.

### Integration Testing

Integration testing verifies communication between external wrappers, HARP, and the graphical user interface. These tests answer questions such as:

- Are messages exchanged correctly between components?
- Does the application successfully complete end-to-end workflows?
- Are user interface updates triggered at the appropriate times?

These tests provide confidence that the complete system functions as an integrated application.

### Unit Testing

This framework focuses on validating HARP's internal logic independently of wrapper communication. These tests answer questions such as:

- Is external configuration interpreted correctly?
- Is application state updated consistently?
- Is invalid input rejected appropriately?
- Are internal workflow invariants preserved?

By isolating HARP-owned logic from external communication, unit tests can verify deterministic behavior that may not be directly observable during end-to-end execution.

### Relationship Between the Two

Neither testing strategy is sufficient on its own. Successful integration tests do not guarantee that HARP's internal logic is correct, and successful unit tests do not guarantee that external components communicate correctly. Together, the two approaches provide confidence that both HARP's internal behavior and its interactions with the surrounding system operate as intended.

## Scope

This framework is intentionally limited to validating logic whose correctness is HARP's responsibility. The goal is not to maximize code coverage, but to maximize confidence in the correctness of HARP-owned behavior.

HARP's implementation can be viewed as three architectural responsibilities:

### Decision Logic

Decision logic determines how HARP interprets, validates, and transforms information before it is presented to the user or communicated to external components.

Examples include:

- Parsing configuration objects
- Validating model paths
- Selecting endpoints
- Transforming payloads
- Mapping internal representations

Decision logic is deterministic and is therefore a primary target for unit testing.

### State Management

State management maintains information that must remain consistent throughout the lifetime of the application.

Examples include:

- Application settings
- API key management
- Internal configuration state

These behaviors are verified using both function-centric and workflow-centric unit tests to ensure that state remains consistent across multiple operations.

### Communication

Communication is responsible for exchanging information between HARP and external systems.

Examples include:

- Wrapper interfaces
- HTTP communication
- GUI event propagation
- Framework callbacks

Although this code is essential to the application, its correctness depends heavily on external libraries, frameworks, and runtime environments. As a result, communication behavior is primarily validated through integration testing rather than isolated unit tests.

### Included in Scope

This framework focuses on testing:

- HARP-owned parsing logic
- HARP-owned validation logic
- HARP-owned decision making
- HARP-owned state management
- Deterministic workflow behavior

### Excluded from Scope

This framework intentionally does not directly test:

- C++ Standard Library behavior
- JUCE framework behavior
- Third-party library behavior
- Wrapper implementations
- GUI rendering
- Network transport
- End-to-end wrapper communication

These components fall outside HARP's responsibility for correctness. This framework therefore focuses on validating the HARP-owned logic that interprets, coordinates, and manages their use.

## Core Testing Philosophy

This framework is guided by the following principles. Every test added to the HARP unit test suite should be consistent with these principles.

### 1. Test HARP-Owned Logic

Unit tests should validate only logic whose correctness is HARP's responsibility. External libraries, frameworks, and wrapper implementations are trusted to validate their own behavior unless HARP extends or transforms that behavior.

### 2. Test Invariants, Not Implementations

Tests should protect the observable properties that HARP guarantees rather than specific implementation details. This allows internal implementations to evolve while preserving expected behavior.

### 3. Prioritize Confidence Over Coverage

The objective of this framework is not to maximize code coverage, but to maximize confidence in HARP's correctness. A small number of high-value behavioral tests provides stronger evidence of correctness than a large number of low-value implementation tests.

### 4. Prefer Behavioral and Workflow Testing

Whenever possible, tests should verify complete behaviors rather than isolated implementation details. Function-centric tests are appropriate for deterministic parsing and validation logic, while workflow-centric tests are preferred for state management and multi-step behaviors.

### 5. Respect Testing Boundaries

Framework behavior, standard library functionality, network communication, and wrapper communication should not be unit tested unless HARP introduces additional logic that changes or extends their behavior.

### 6. Every Test Must Increase Confidence

Every included test should provide meaningful additional confidence in HARP's correctness. Tests that merely duplicate guarantees already provided by another test or by trusted external libraries should be excluded.

## HARP Testing Architecture

From a testing perspective, HARP can be viewed as three primary areas of responsibility:

```text
                 External Wrappers
                         │
                         ▼

        +----------------------------------+
        |              HARP                |
        |----------------------------------|
        |                                  |
        |  Decision Logic      ✓ Unit      |
        |  State Management    ✓ Unit      |
        |----------------------------------|
        |  Communication       Integration |
        +----------------------------------+

                         │
                         ▼

                   Graphical Interface
```


## Testing Methodology

This framework follows a structured methodology for determining whether a function should be unit tested. Rather than beginning with implementation, every candidate function is evaluated using a consistent audit process that identifies its role within HARP and determines whether unit testing provides meaningful additional confidence.

### Step 1 — Identify HARP-Owned Logic

Determine whether the behavior being evaluated is HARP's responsibility.

Questions to consider include:

- Does HARP define or transform this behavior?
- Is the behavior independent of external frameworks?
- Would an error in this logic represent a defect in HARP?

If the answer is **no**, the function is excluded from unit testing.

---

### Step 2 — Identify the Protected Invariant

Determine the observable property that HARP guarantees to preserve. This property becomes the invariant protected by the test.

Examples include:

- Configuration is parsed correctly.
- Invalid input is rejected.
- Application state remains consistent.
- Workflow behavior remains deterministic.

Every selected test must protect at least one documented invariant.

---

### Step 3 — Evaluate Behavioral Importance

Determine whether incorrect behavior would meaningfully affect HARP's correctness or the user's experience.

A useful heuristic is:

> If this behavior were incorrect, could integration testing still appear to succeed while the user experiences incorrect behavior?

If the answer is **yes**, that is strong evidence that the behavior belongs in the unit test suite.

Functions that merely forward data, wrap external libraries, or duplicate framework behavior generally provide little additional confidence when unit tested.

---

### Step 4 — Assign Priority

Each selected function is assigned a priority level (P1–P4) based on the impact of failure.

The priority determines implementation order and helps ensure that the highest-value behaviors are verified first.

---

### Step 5 — Select the Appropriate Test Strategy

Choose the testing strategy that best validates the protected invariant.

Examples include:

- Function-centric tests for deterministic parsing and validation logic.
- Workflow-centric tests for state management.
- Boundary tests for edge cases.
- Behavioral tests for observable application behavior.

The selected strategy should maximize confidence while minimizing redundant validation.

---

### Step 6 — Document the Result

Every evaluated function receives one of the following classifications:

- **Selected** — Included in the unit test suite.
- **Deferred** — Better validated through integration testing.
- **Excluded** — Outside HARP's responsibility (for example, standard library or framework behavior).
- **Rejected** — HARP-owned logic that does not provide meaningful additional confidence if unit tested.

These classifications, together with the associated rationale, are recorded in the accompanying project documentation.

This methodology ensures that every included test is selected intentionally, justified by documented rationale, and directly connected to HARP's correctness.

## Invariant Levels

Every selected unit test is associated with an invariant level.

An invariant represents a property that HARP guarantees to preserve during execution. Rather than organizing tests by implementation details, this framework organizes them by the behavioral guarantees they protect.

Invariant levels describe *what* a test protects. Test categories describe *how* that protection is verified.

### Parsing

Parsing invariants ensure that external information is interpreted correctly and transformed into valid HARP objects.

**Primary objective:** Ensure external information is interpreted correctly.

Examples include:

- Configuration parsing
- Dynamic object construction
- Internal data representation

---

### State

State invariants ensure that application data remains internally consistent throughout execution.

**Primary objective:** Preserve internal consistency.

Examples include:

- Application settings
- API key management
- Cached configuration

State invariants are often verified using workflow-centric tests because correctness depends on sequences of operations rather than isolated function calls.

---

### Validation

Validation invariants ensure that invalid information is detected and prevented from entering the system.

**Primary objective:** Prevent invalid information from entering the system.

Examples include:

- Model path validation
- Request validation
- Response validation

Validation tests focus on both accepted inputs and rejected inputs, with particular attention given to edge cases and malformed data.

---

### Workflow

Workflow invariants ensure that multiple operations cooperate correctly to preserve expected system behavior.

**Primary objective:** Preserve correctness across multiple operations.

Examples include:

- Configuration lifecycle
- API key lifecycle
- Multi-step state transitions

Workflow tests verify behaviors that cannot be adequately demonstrated through isolated function tests alone.

---

### Contract

Contract invariants ensure that shared representations remain stable throughout the application.

**Primary objective:** Maintain stable shared representations.

Examples include:

- Enumeration mappings
- Shared identifiers
- Stable internal representations

Although contract logic is often simple, violations can silently affect multiple parts of the application because many components depend on the same shared contract.

## Priority Levels

Every function selected for evaluation is assigned a priority level before implementation begins.

Priority levels are determined by the consequences that incorrect behavior would have on HARP's correctness. Priority is assigned to individual behaviors rather than entire source files. A single file may contain functions with different priorities depending on the invariants they preserve and the impact of failure.

Priority levels establish implementation order and help ensure that testing effort is focused on the behaviors that provide the greatest increase in confidence.

| Priority | Description |
|----------|-------------|
| **P1** | Failure compromises a core HARP invariant and may produce incorrect application behavior despite successful wrapper communication. These functions must be unit tested. |
| **P2** | Failure affects important application behavior or user experience but is less likely to compromise overall correctness. These functions should generally be unit tested. |
| **P3** | Failure has limited impact or is partially covered by higher-priority behaviors. These functions may be tested if they provide meaningful additional confidence. |
| **P4** | Failure provides little additional confidence, duplicates existing guarantees, or primarily involves communication or framework behavior. These functions are generally excluded from unit testing. |

### Priority Assignment

Priority is determined by evaluating questions such as:

- Does this function preserve a documented invariant?
- Would incorrect behavior affect the user's experience?
- Could integration testing succeed while this behavior remains incorrect?
- Would excluding this test reduce confidence in HARP's correctness?

Priority should always be assigned before implementation. The resulting priority and its rationale should be documented in `TEST_MATRIX.md`.

## Test Categories

Test categories describe how a protected invariant is verified. Unlike invariant levels, which describe the behavioral guarantee being protected, test categories describe the testing strategy used to verify that guarantee.

Multiple test categories may be used to verify the same invariant when doing so provides meaningful additional confidence.

The selected test category should be the simplest strategy capable of verifying the protected invariant. More complex testing strategies should only be used when simpler strategies cannot adequately demonstrate correctness.

### Function-Centric Tests

Function-centric tests verify deterministic behavior that can be evaluated independently of the surrounding system.

**Primary objective:** Verify deterministic behavior in isolation.

Typical examples include:

- Parsing logic
- Validation logic
- Data transformation
- Deterministic algorithms

These tests are generally concise and verify a single behavioral expectation.

---

### Workflow-Centric Tests

Workflow-centric tests verify behaviors that emerge through sequences of operations rather than isolated function calls.

**Primary objective:** Verify correctness across multiple operations.

Typical examples include:

- Settings lifecycle
- API key management
- Multi-step state transitions
- Configuration persistence

These tests ensure that application state remains consistent throughout realistic usage scenarios.

---

### Boundary Tests

Boundary tests verify behavior at the limits of expected input.

**Primary objective:** Verify behavior at the limits of valid input.

Typical examples include:

- Empty inputs
- Missing values
- Maximum and minimum values
- Very large inputs
- Unicode input
- Malformed structures

Boundary testing provides confidence that deterministic logic remains correct under unusual but valid operating conditions.

---

### Validation Tests

Validation tests verify that invalid or unsupported input is correctly detected and handled.

**Primary objective:** Verify rejection of invalid input.

Typical examples include:

- Invalid model paths
- Unsupported configuration
- Malformed requests
- Invalid response structures

Validation tests should verify both accepted and rejected inputs whenever practical.

---

### Contract Tests

Contract tests verify that shared representations remain stable throughout the application.

**Primary objective:** Verify stability of shared representations.

Typical examples include:

- Enumeration mappings
- Shared identifiers
- Stable serialization formats

Although contract logic is often simple, failures can affect multiple independent components because many parts of the application depend on the same shared representation.

## Function Selection Process

Before implementing a new unit test, every candidate function should undergo the following evaluation process.

The purpose of this process is to ensure that every included test is intentional, justified, and contributes meaningful additional confidence in HARP's correctness.

```text
                Candidate Function
                        │
                        ▼
             Is the behavior HARP-owned?
                 │               │
                No              Yes
                 │               ▼
           Excluded      Does it contain
                         business logic?
                             │      │
                            No     Yes
                             │      ▼
                        Rejected  Is the
                                  behavior
                               deterministic?
                                  │      │
                                 No     Yes
                                  │      ▼
                            Deferred  Is the
                                      behavior
                                     observable?
                                      │      │
                                     No     Yes
                                      │      ▼
                                Reevaluate  Identify
                                            invariant
                                                │
                                                ▼
                                         Assign priority
                                                │
                                                ▼
                                        Select simplest
                                       sufficient test
                                            strategy
                                                │
                                                ▼
                                          Implement
                                                │
                                                ▼
                                    Update TEST_MATRIX.md
                                                │
                                                ▼
                              Update AUDIT_NOTES.md if needed
                                                │
                                                ▼
                              Update DECISIONS.md if methodology
                                           changes
```

Classification decisions should be conservative. When uncertainty exists, prefer classifying a function as Deferred or Rejected until additional evidence justifies inclusion. The objective is not to maximize the number of tests, but to maximize the confidence provided by the tests that are included.

### Step 1 — Determine Ownership

Determine whether the behavior being evaluated is HARP's responsibility.

If correctness belongs to an external framework, library, or wrapper implementation, the function should be classified as **Excluded**.

---

### Step 2 — Evaluate Business Logic

Determine whether the function contributes meaningful HARP-owned behavior.

Functions that merely forward data, wrap external APIs without modification, or duplicate framework behavior generally do not warrant direct unit tests.

---

### Step 3 — Evaluate Determinism

Determine whether the behavior can be reproduced consistently under controlled conditions.

Behavior that depends primarily on asynchronous communication, networking, or external runtime environments is generally better suited for integration testing and should be classified as **Deferred**.

---

### Step 4 — Evaluate Observability

Determine whether incorrect behavior can be observed through deterministic outputs or preserved invariants.

If correctness cannot be meaningfully observed, reconsider whether the function should be tested directly or whether the invariant is better verified elsewhere.

---

### Step 5 — Identify the Protected Invariant

Determine the behavioral guarantee that the function preserves.

Every selected test must protect at least one documented invariant.

---

### Step 6 — Assign Priority

Assign a priority level (P1-P4) based on the consequences of failure.

Priority should be assigned to the behavior being tested rather than the source file that contains it.

---

### Step 7 — Select the Simplest Sufficient Test Strategy

Choose the simplest testing strategy capable of verifying the protected invariant.

More complex testing strategies should only be used when simpler strategies cannot adequately demonstrate correctness.

---

### Step 8 — Record the Decision

Every evaluated function should receive one of the following classifications:

| Classification | Description |
|---------------|-------------|
| **Selected** | Included in the unit test suite. |
| **Deferred** | Better validated through integration testing. |
| **Excluded** | Outside HARP's responsibility for correctness. |
| **Rejected** | HARP-owned logic that does not provide meaningful additional confidence if unit tested. |

The rationale for every classification should be documented to maintain traceability between the audit process and the implemented test suite.

## Documentation

This testing framework is supported by four companion documents. Together, they provide the rationale, traceability, and engineering context for the unit test suite.

| Document | Purpose |
|----------|---------|
| **README.md** | Defines the testing framework specification, methodology, and implementation guidelines. |
| **DECISIONS.md** | Records architectural decisions that define how the testing framework is designed, implemented, and maintained. |
| **AUDIT_NOTES.md** | Documents observations made during source code audits, including implementation discoveries, potential issues, confidence assessments, and function classifications. |
| **TEST_MATRIX.md** | Provides traceability between audited source code and the implemented unit test suite, including priorities, invariant levels, test categories, and implementation status. |

These documents serve different purposes and should be maintained independently. Changes to one document should not duplicate information maintained by another unless doing so improves clarity or traceability.

Collectively, these documents ensure that implementation, engineering rationale, architectural decisions, and traceability remain synchronized as the testing framework evolves. Maintaining this separation of responsibilities helps prevent duplication while providing a complete record of how and why the framework was designed.

## Repository Structure

The testing framework is organized as follows:

```text
tests/
│
├── README.md
├── DECISIONS.md
├── AUDIT_NOTES.md
├── TEST_MATRIX.md
│
├── utils/
├── clients/
├── fixtures/
└── data/
```

### Directory Responsibilities

- **utils/** — Unit tests for utility components and deterministic parsing logic.
- **clients/** — Unit tests for client implementations and decision logic.
- **fixtures/** — Shared helper objects, factories, mocks, and reusable testing utilities.
- **data/** — Shared test data, sample configurations, and edge-case inputs used by multiple test suites.

The `fixtures/` and `data/` directories should only be introduced when they reduce duplication or improve maintainability. They should not exist solely for organizational purposes.

## Implementation Guidelines

When implementing new unit tests:

- Identify the protected invariant before writing any test code.
- Test only HARP-owned logic.
- Choose the simplest sufficient test strategy.
- Prefer behavioral evidence over implementation details.
- Assign a priority before implementation.
- Document the rationale for every included test.
- Avoid testing external libraries or framework behavior.
- Reuse fixtures and shared test data whenever practical.
- Favor readability and maintainability over maximizing the number of tests.
- Keep tests deterministic and independent whenever possible.

## Contributing New Tests

Before adding a new unit test:

1. Audit the candidate function using the documented Function Selection Process.
2. Confirm that the behavior is HARP-owned.
3. Identify the protected invariant.
4. Assign an appropriate priority level.
5. Select the simplest sufficient test strategy.
6. Implement the test.
7. Update `TEST_MATRIX.md` to maintain traceability.
8. Update `AUDIT_NOTES.md` if implementation discoveries or potential issues are identified.
9. Update `DECISIONS.md` if the testing methodology changes.

Every contribution should improve confidence in HARP's correctness while remaining consistent with the principles defined by this framework.
