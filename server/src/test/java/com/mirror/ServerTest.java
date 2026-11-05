package com.mirror;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.*;

public class ServerTest {

    @Test
    public void testParseArgsDefault() {
        Server.Options options = Server.parseArgs(new String[0]);
        assertEquals("mirror", options.getSocketName());
        assertEquals(0, options.getMaxSize());
        assertEquals(8_000_000, options.getBitRate());
        assertFalse(options.isTunnelForward());
    }

    @Test
    public void testParseArgsWithEquals() {
        String[] args = {
                "--socket-name=custom_socket",
                "--max-size=1080",
                "--bit-rate=4000000",
                "--tunnel-forward=true"
        };
        Server.Options options = Server.parseArgs(args);
        assertEquals("custom_socket", options.getSocketName());
        assertEquals(1080, options.getMaxSize());
        assertEquals(4_000_000, options.getBitRate());
        assertTrue(options.isTunnelForward());
    }

    @Test
    public void testParseArgsWithSpaces() {
        String[] args = {
                "--socket-name", "custom_socket2",
                "--max-size", "1440",
                "--bit-rate", "12000000",
                "--tunnel-forward", "true"
        };
        Server.Options options = Server.parseArgs(args);
        assertEquals("custom_socket2", options.getSocketName());
        assertEquals(1440, options.getMaxSize());
        assertEquals(12_000_000, options.getBitRate());
        assertTrue(options.isTunnelForward());
    }
}
