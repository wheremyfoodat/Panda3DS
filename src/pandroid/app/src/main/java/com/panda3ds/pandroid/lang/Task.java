package com.panda3ds.pandroid.lang;

public class Task extends Thread {
    public Task(Runnable runnable) {
        super(runnable);
    }

    public void runSync() {
        start();
        waitFinish();
    }

    public void waitFinish() {
        try {
            join();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }
}
