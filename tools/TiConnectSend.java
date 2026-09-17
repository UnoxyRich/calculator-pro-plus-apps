import com.ti.et.elg.connectServer.commManager.ConnectServerTestApp;
import com.ti.et.elg.connectServer.exports.TestAutomationInterface;
import com.ti.et.elg.connectServer.exports.TIVar;
import com.ti.et.elg.connectServer.factory.ConnectServerFactory;
import com.ti.et.utils.logger.TILogger;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.logging.Level;

/** Transfers only PHYS1. It cannot delete, execute, or press calculator keys. */
public final class TiConnectSend {
    private static void fail(String message) {
        throw new IllegalStateException(message);
    }

    private static TIVar findProgram(List<TIVar> directory) {
        for (TIVar variable : directory) {
            if (variable.getVarType() == TIVar.typeTI83PlusProgram
                    && "PHYS1".equals(variable.getDeviceFileName())) {
                return variable;
            }
        }
        return null;
    }

    private static List<TIVar> directory(TestAutomationInterface api,
                                         String connection) {
        List<TIVar> result = new ArrayList<>();
        int code = api.getDirectory(connection, result);
        if (code != 0) fail("Directory read failed: " + code);
        return result;
    }

    private static byte[] receive(TestAutomationInterface api, String connection,
                                  TIVar variable, Path output) throws Exception {
        if (Files.exists(output)) fail("Refusing to overwrite " + output);
        variable.setHostFile(output.toFile());
        int code = api.getVariable(connection, variable);
        if (code != 0 || !Files.isRegularFile(output) || Files.size(output) == 0) {
            fail("Read-back failed: " + code);
        }
        TIVar received = api.createTIVar(output.toString());
        if (received == null || received.getData() == null) {
            fail("Could not parse read-back file");
        }
        return received.getData();
    }

    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.println("usage: TiConnectSend PHYS1.8xp output-directory");
            System.exit(2);
        }
        File input = new File(args[0]).getAbsoluteFile();
        if (!input.isFile() || !input.getName().equals("PHYS1.8xp")) {
            fail("Only an existing PHYS1.8xp file is accepted");
        }
        Path outputDir = Path.of(args[1]).toAbsolutePath();
        Files.createDirectories(outputDir);
        Path logDir = outputDir.resolve("ti-connect-logs");
        Files.createDirectories(logDir);

        TILogger.init(Level.INFO, logDir.toString(), true);
        ConnectServerTestApp.initConnectServerTestApp(logDir.toString(), false, false);
        TestAutomationInterface api = ConnectServerFactory.getTestAutomationAPI();
        try {
            TIVar expected = api.createTIVar(input.getPath());
            if (expected == null || expected.getVarType() != TIVar.typeTI83PlusProgram
                    || !"PHYS1".equals(expected.getDeviceFileName())
                    || expected.getData() == null || expected.getData().length == 0) {
                fail("Input is not a valid single PHYS1 program");
            }

            List<String> connections = new ArrayList<>();
            int code = -1;
            for (int attempt = 0; attempt < 20; attempt++) {
                connections.clear();
                code = api.getConnectedDevices(connections);
                if (code == 0 && !connections.isEmpty()) break;
                Thread.sleep(500L);
            }
            if (code != 0 || connections.size() != 1) {
                fail("Expected exactly one connected calculator; code=" + code
                        + ", count=" + connections.size());
            }
            String connection = connections.get(0);
            System.out.println("Connected: " + connection);

            List<TIVar> before = directory(api, connection);
            for (TIVar variable : before) {
                if ("PHYS1L".equals(variable.getDeviceFileName())) {
                    System.out.println("Warning: old PHYS1L is still present; it will not be touched.");
                }
            }
            TIVar existing = findProgram(before);
            if (existing != null) {
                Path backup = outputDir.resolve("PHYS1-before-install-"
                        + System.currentTimeMillis() + ".8xp");
                byte[] current = receive(api, connection, existing, backup);
                System.out.println("Existing PHYS1 saved to " + backup);
                if (Arrays.equals(expected.getData(), current)) {
                    System.out.println("ALREADY_INSTALLED: exact program data matches.");
                    return;
                }
                fail("Different PHYS1 already exists. Refusing an unverified overwrite.");
            }

            System.out.println("Sending PHYS1 (" + expected.getData().length + " data bytes).");
            code = api.sendVariable(connection, expected);
            if (code != 0) fail("Transfer failed: " + code);

            TIVar installed = null;
            for (int attempt = 0; attempt < 10; attempt++) {
                installed = findProgram(directory(api, connection));
                if (installed != null) break;
                Thread.sleep(500L);
            }
            if (installed == null) fail("Transfer returned success but PHYS1 is absent.");
            Path readback = outputDir.resolve("PHYS1-readback-"
                    + System.currentTimeMillis() + ".8xp");
            byte[] actual = receive(api, connection, installed, readback);
            if (!Arrays.equals(expected.getData(), actual)) {
                fail("Read-back content differs from the built program; file=" + readback);
            }
            System.out.println("INSTALLED: PHYS1 read-back matches exactly; file=" + readback);
        } finally {
            api.shutDown();
        }
    }
}
