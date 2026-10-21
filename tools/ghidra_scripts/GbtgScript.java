// Shared path helpers for GbtG headless scripts.
//@category GbtG

import java.io.File;
import ghidra.app.script.GhidraScript;

/**
 * Resolves the repo that contains this script ({@code tools/ghidra/ghidra_scripts}),
 * not a hardcoded drive letter. Override with the {@code GBTG_ROOT} environment variable.
 */
public class GbtgScript extends GhidraScript {
    @Override
    public void run() throws Exception {
        println("repo " + repoRoot().getAbsolutePath());
    }

    protected File repoRoot() {
        String env = System.getenv("GBTG_ROOT");
        if (env != null && env.trim().length() > 0) {
            return new File(env.trim());
        }
        File fromScript = findRepo(sourceFile == null ? null : new java.io.File(sourceFile.getAbsolutePath()).getParentFile());
        if (fromScript != null) {
            return fromScript;
        }
        File fromCwd = findRepo(new File(System.getProperty("user.dir")));
        if (fromCwd != null) {
            return fromCwd;
        }
        throw new IllegalStateException("GbtG repo root not found. Set GBTG_ROOT.");
    }

    protected File cacheFile(String name) {
        File cache = new File(repoRoot(), "cache");
        if (!cache.isDirectory() && !cache.mkdirs()) {
            throw new IllegalStateException("could not create " + cache.getAbsolutePath());
        }
        return new File(cache, name);
    }

    protected File repoFile(String relative) {
        File file = new File(repoRoot(), relative.replace('/', File.separatorChar));
        File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IllegalStateException("could not create " + parent.getAbsolutePath());
        }
        return file;
    }

    private static File findRepo(File start) {
        File dir = start;
        for (int i = 0; i < 8 && dir != null; i++) {
            if (new File(dir, "CMakeLists.txt").isFile() && new File(dir, "src").isDirectory()) {
                return dir;
            }
            dir = dir.getParentFile();
        }
        return null;
    }
}
