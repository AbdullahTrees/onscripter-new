package org.umineko_project.onscripter_ru;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;

import java.io.File;

public class GameStorageTest {
    @Test
    public void appScopedRootKeepsLegacyParentBase() {
        File files = new File("/storage/emulated/0/Android/data/example/files");
        File scoped = new File(files, GameStorage.PROVIDER_DIR);

        assertEquals(files, GameStorage.selectNativeStorageBase(scoped, scoped));
    }

    @Test
    public void selectedRootIsItsOwnNativeBase() {
        File scoped = new File("/storage/emulated/0/Android/data/example/files/ONScripter-RU");
        File selected = new File("/storage/emulated/0/Games/Umineko");

        assertEquals(selected, GameStorage.selectNativeStorageBase(selected, scoped));
    }

    @Test
    public void missingRootHasNoNativeBase() {
        assertNull(GameStorage.selectNativeStorageBase(null, new File("/unused")));
    }
}
