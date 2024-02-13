package com.panda3ds.pandroid.lang;

import java.io.InputStream;
import java.io.OutputStream;

public class PipeStreamTask extends Task {
    private final InputStream input;
    private final OutputStream output;
    private final long limit;
    private long size;

    public PipeStreamTask(InputStream input, OutputStream output, long limit) {
        this.input = input;
        this.output = output;
        this.limit = limit;
    }

    @Override
    public void run() {
        super.run();
        int data;
        try {
            while ((data = input.read()) != -1) {
                output.write(data);
                if (++size > limit) {
                    break;
                }
            }
        } catch (Exception e) {}
        close();
    }

    public void close() {
        try {
            output.flush();
            output.close();
            input.close();
        } catch (Exception e) {}
    }
}
