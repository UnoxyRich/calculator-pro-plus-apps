import com.ti.et.elg.connectServer.commManager.ConnectServerTestApp;
import com.ti.et.elg.connectServer.exports.TestAutomationInterface;
import com.ti.et.elg.connectServer.exports.TIVar;
import com.ti.et.elg.connectServer.factory.ConnectServerFactory;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Level;

import com.ti.et.utils.logger.TILogger;

public final class TiConnectCeCli {
    private static final String DATA_DIR = "/tmp/ti-connect-ce-cli";

    private static void fail(String message) {
        throw new IllegalStateException(message);
    }

    public static void main(String[] args) throws Exception {
        if (args.length == 1 && "list".equals(args[0])) {
            listFiles();
            return;
        }
        if (args.length < 2 || !("send".equals(args[0]) || "install".equals(args[0])
                || "reinstall".equals(args[0]) || "remove".equals(args[0])
                || "run".equals(args[0]))) {
            System.err.println("usage: TiConnectCeCli send|install|reinstall|remove|run <file>...");
            System.exit(2);
        }

        if ("install".equals(args[0])) {
            installFiles(args);
            return;
        }
        if ("reinstall".equals(args[0])) {
            reinstallFiles(args);
            return;
        }
        if ("remove".equals(args[0])) {
            transferFiles(args, true, true);
            return;
        }

        File hostFile = new File(args[1]).getAbsoluteFile();
        if (!hostFile.isFile()) fail("File does not exist: " + hostFile);

        TILogger.init(Level.INFO, DATA_DIR, true);
        ConnectServerTestApp.initConnectServerTestApp(DATA_DIR, false, false);
        TestAutomationInterface api = ConnectServerFactory.getTestAutomationAPI();
        try {
            List<String> connections = new ArrayList<>();
            int code = -1;
            for (int attempt = 0; attempt < 40; attempt++) {
                code = api.getConnectedDevices(connections);
                if (code == 0 && !connections.isEmpty()) {
                    break;
                }
                Thread.sleep(500L);
            }
            if (connections.isEmpty()) {
                fail("No calculator detected; code=" + code);
            }

            String connection = connections.get(0);
            System.out.println("Detected connection: " + connection);

            TIVar variable = api.createTIVar(hostFile.getPath());
            if (variable == null) {
                fail("TI Connect could not parse " + hostFile);
            }
            System.out.println("Sending: " + variable.getDeviceFileName()
                    + " type=" + variable.getVarType()
                    + " bytes=" + variable.getDataSize());

            if ("send".equals(args[0])) {
                code = api.sendVariable(connection, variable);
                if (code != 0) {
                    fail("TI Connect send failed with code " + code);
                }
                System.out.println("Send returned success.");
            } else {
                code = api.executeVariable(connection, variable);
                if (code != 0) {
                    fail("TI Connect execute failed with code " + code);
                }
                System.out.println("Execute returned success.");
                Thread.sleep(5000L);
                File screenFile = new File("tmp/physical-screen-ti-connect.png")
                        .getAbsoluteFile();
                if (!screenFile.getParentFile().isDirectory()
                        && !screenFile.getParentFile().mkdirs()) {
                    fail("Could not create screen output directory");
                }
                code = api.getScreen(connection, screenFile.getPath());
                if (code != 0) {
                    fail("TI Connect screen capture failed with code " + code);
                }
                System.out.println("Screen captured to " + screenFile);
                return;
            }

            List<TIVar> directory = new ArrayList<>();
            code = api.getDirectory(connection, directory);
            if (code != 0) {
                fail("TI Connect directory read failed with code " + code);
            }

            boolean found = false;
            for (TIVar item : directory) {
                System.out.println("Device variable: " + item.getDeviceFileName());
                if ("PHYS1".equalsIgnoreCase(item.getDeviceFileName())) {
                    found = true;
                }
            }
            if (!found) {
                fail("PHYS1 was not returned by the calculator directory.");
            }
            System.out.println("PHYS1 confirmed on the calculator.");
        } finally {
            api.shutDown();
        }
    }

    private static void installFiles(String[] args) throws Exception {
        transferFiles(args, false, false);
    }

    private static void listFiles() throws Exception {
        TILogger.init(Level.INFO, DATA_DIR, true);
        ConnectServerTestApp.initConnectServerTestApp(DATA_DIR, false, false);
        TestAutomationInterface api = ConnectServerFactory.getTestAutomationAPI();
        try {
            List<String> connections = new ArrayList<>();
            int code = -1;
            for (int attempt = 0; attempt < 40; attempt++) {
                code = api.getConnectedDevices(connections);
                if (code == 0 && !connections.isEmpty()) break;
                Thread.sleep(500L);
            }
            if (connections.isEmpty()) fail("No calculator detected; code=" + code);
            List<TIVar> directory = new ArrayList<>();
            code = api.getDirectory(connections.get(0), directory);
            if (code != 0) fail("TI Connect directory read failed with code " + code);
            for (TIVar variable : directory) {
                System.out.println(variable.getDeviceFileName() + " type="
                        + variable.getVarType() + " archived="
                        + variable.isVarFlagArchive() + " bytes="
                        + variable.getDataSize());
            }
        } finally {
            api.shutDown();
        }
    }

    private static void reinstallFiles(String[] args) throws Exception {
        transferFiles(args, true, false);
    }

    private static void transferFiles(String[] args, boolean removeFirst,
                                      boolean removeOnly) throws Exception {
        List<File> files = new ArrayList<>();
        for (int i = 1; i < args.length; i++) {
            File file = new File(args[i]).getAbsoluteFile();
            if (!file.isFile()) fail("File does not exist: " + file);
            files.add(file);
        }

        TILogger.init(Level.INFO, DATA_DIR, true);
        ConnectServerTestApp.initConnectServerTestApp(DATA_DIR, false, false);
        TestAutomationInterface api = ConnectServerFactory.getTestAutomationAPI();
        try {
            List<String> connections = new ArrayList<>();
            int code = -1;
            for (int attempt = 0; attempt < 40; attempt++) {
                code = api.getConnectedDevices(connections);
                if (code == 0 && !connections.isEmpty()) break;
                Thread.sleep(500L);
            }
            if (connections.isEmpty()) fail("No calculator detected; code=" + code);
            String connection = connections.get(0);
            System.out.println("Detected connection: " + connection);

            for (File file : files) {
                TIVar variable = api.createTIVar(file.getPath());
                if (variable == null) fail("TI Connect could not parse " + file);
                if (removeFirst) {
                    System.out.println("Removing existing: " + variable.getDeviceFileName());
                    code = api.deleteVariable(connection, variable);
                    if (code != 0) {
                        fail("TI Connect delete failed for "
                                + variable.getDeviceFileName() + " with code " + code);
                    }
                    System.out.println("Removed: " + variable.getDeviceFileName());
                }
                if (removeOnly) continue;
                System.out.println("Installing: " + variable.getDeviceFileName()
                        + " type=" + variable.getVarType()
                        + " bytes=" + variable.getDataSize());
                code = api.sendVariable(connection, variable);
                if (code != 0) fail("TI Connect send failed with code " + code);
                System.out.println("Transfer returned success for "
                        + variable.getDeviceFileName());
            }
        } finally {
            api.shutDown();
        }
    }
}
