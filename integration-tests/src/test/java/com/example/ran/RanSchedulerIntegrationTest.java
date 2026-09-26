package com.example.ran;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assertions.fail;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class RanSchedulerIntegrationTest {
  private static final ObjectMapper JSON = new ObjectMapper();
  private static final long PROCESS_TIMEOUT_SECONDS = 30;

  private static Path executable;
  private static Path repositoryRoot;
  private static Path basicConfig;

  @TempDir Path temporaryDirectory;

  @BeforeAll
  static void validateProjectPaths() {
    executable = requiredPathProperty("ran.executable");
    repositoryRoot = requiredPathProperty("ran.repository.root");
    basicConfig = repositoryRoot.resolve("configs/basic.json");

    if (!Files.isRegularFile(executable) || !Files.isExecutable(executable)) {
      throw new IllegalStateException(
          "Maven property ran.executable must point to an executable file: " + executable);
    }
    if (!Files.isDirectory(repositoryRoot)) {
      throw new IllegalStateException(
          "Maven property ran.repository.root must point to a directory: " + repositoryRoot);
    }
    if (!Files.isRegularFile(basicConfig)) {
      throw new IllegalStateException("Could not find configs/basic.json under " + repositoryRoot);
    }
  }

  @Test
  void helpCommandDescribesTheSimulatorAndSchedulers() throws Exception {
    ProcessResult result = run("--help");

    assertProcessSucceeded(result);
    assertTrue(
        result.standardOutput().contains("simplified educational 5G RAN scheduling simulator"),
        () -> "Help output did not include the project description:\n" + result.standardOutput());
    assertTrue(
        result.standardOutput().contains("round-robin"),
        () -> "Help output did not list round-robin:\n" + result.standardOutput());
    assertTrue(
        result.standardOutput().contains("proportional-fair"),
        () -> "Help output did not list proportional-fair:\n" + result.standardOutput());
  }

  @Test
  void roundRobinRunCreatesExpectedResults() throws Exception {
    Path outputDirectory = temporaryDirectory.resolve("round-robin");

    JsonNode summary = runAndVerifySimulation("round-robin", outputDirectory);

    assertEquals("round-robin", summary.path("scheduler_name").asText());
  }

  @Test
  void proportionalFairRunCreatesExpectedResults() throws Exception {
    Path outputDirectory = temporaryDirectory.resolve("proportional-fair");

    JsonNode summary = runAndVerifySimulation("proportional-fair", outputDirectory);

    assertEquals("proportional-fair", summary.path("scheduler_name").asText());
  }

  @Test
  void repeatedRunsProduceIdenticalFiles() throws Exception {
    Path firstOutput = temporaryDirectory.resolve("first");
    Path secondOutput = temporaryDirectory.resolve("second");
    runAndVerifySimulation("round-robin", firstOutput);
    runAndVerifySimulation("round-robin", secondOutput);

    for (String filename : List.of("summary.json", "per_ue.csv", "per_slot.csv")) {
      assertArrayEquals(
          Files.readAllBytes(firstOutput.resolve(filename)),
          Files.readAllBytes(secondOutput.resolve(filename)),
          "Deterministic runs produced different " + filename + " files");
    }
  }

  @Test
  void missingConfigOptionIsRejected() throws Exception {
    assertProcessFailedWith(run(), "--config is required");
  }

  @Test
  void missingConfigValueIsRejected() throws Exception {
    assertProcessFailedWith(run("--config"), "missing value for --config");
  }

  @Test
  void unsupportedSchedulerIsRejected() throws Exception {
    assertProcessFailedWith(
        run("--config", basicConfig.toString(), "--scheduler", "not-a-scheduler"),
        "unsupported scheduler");
  }

  @Test
  void unknownOptionIsRejected() throws Exception {
    assertProcessFailedWith(
        run("--config", basicConfig.toString(), "--unknown"),
        "unknown command-line option");
  }

  @Test
  void nonexistentConfigurationFileIsRejected() throws Exception {
    Path missingConfig = temporaryDirectory.resolve("missing.json");

    assertProcessFailedWith(run("--config", missingConfig.toString()), "cannot open file");
  }

  private JsonNode runAndVerifySimulation(String scheduler, Path outputDirectory)
      throws Exception {
    ProcessResult result =
        run(
            "--config",
            basicConfig.toString(),
            "--scheduler",
            scheduler,
            "--output-dir",
            outputDirectory.toString());
    assertProcessSucceeded(result);
    assertTrue(
        result.standardOutput().contains("Simulation completed successfully"),
        () -> "Success message was missing:\n" + result.describe());

    Path summaryPath = outputDirectory.resolve("summary.json");
    assertTrue(Files.isRegularFile(summaryPath), "summary.json was not created");
    assertTrue(
        Files.isRegularFile(outputDirectory.resolve("per_ue.csv")),
        "per_ue.csv was not created");
    assertTrue(
        Files.isRegularFile(outputDirectory.resolve("per_slot.csv")),
        "per_slot.csv was not created");

    JsonNode summary = JSON.readTree(summaryPath.toFile());
    assertTrue(
        summary.path("total_transmitted_bytes").asLong() > 0,
        "Expected transmitted bytes to be greater than zero");
    assertMetricInUnitRange(summary, "resource_block_utilization");
    assertMetricInUnitRange(summary, "jains_fairness_index");
    return summary;
  }

  private static void assertMetricInUnitRange(JsonNode summary, String fieldName) {
    double value = summary.path(fieldName).asDouble(Double.NaN);
    assertTrue(
        value >= 0.0 && value <= 1.0,
        () -> fieldName + " must be between zero and one, but was " + value);
  }

  private static Path requiredPathProperty(String name) {
    String value = System.getProperty(name);
    if (value == null || value.isBlank()) {
      throw new IllegalStateException("Required Maven property is missing: " + name);
    }
    return Path.of(value).toAbsolutePath().normalize();
  }

  private ProcessResult run(String... arguments) throws IOException, InterruptedException {
    List<String> command = new ArrayList<>();
    command.add(executable.toString());
    command.addAll(Arrays.asList(arguments));

    Process process = new ProcessBuilder(command).directory(repositoryRoot.toFile()).start();
    if (!process.waitFor(PROCESS_TIMEOUT_SECONDS, TimeUnit.SECONDS)) {
      process.destroyForcibly();
      fail("Simulator process timed out after " + PROCESS_TIMEOUT_SECONDS + " seconds: " + command);
    }

    String standardOutput =
        new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8);
    String standardError =
        new String(process.getErrorStream().readAllBytes(), StandardCharsets.UTF_8);
    return new ProcessResult(command, process.exitValue(), standardOutput, standardError);
  }

  private static void assertProcessSucceeded(ProcessResult result) {
    assertEquals(0, result.exitCode(), () -> "Simulator command failed:\n" + result.describe());
  }

  private static void assertProcessFailedWith(ProcessResult result, String expectedMessage) {
    assertTrue(
        result.exitCode() != 0,
        () -> "Simulator command unexpectedly succeeded:\n" + result.describe());
    assertTrue(
        result.standardError().contains(expectedMessage),
        () ->
            "Expected standard error to contain '"
                + expectedMessage
                + "':\n"
                + result.describe());
  }

  private record ProcessResult(
      List<String> command, int exitCode, String standardOutput, String standardError) {
    String describe() {
      return "command: "
          + command
          + "\nexit code: "
          + exitCode
          + "\nstandard output:\n"
          + standardOutput
          + "\nstandard error:\n"
          + standardError;
    }
  }
}
