package com.panda3ds.pandroid.utils;

import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

public class SearchAgent {
    // Store all results in a hashmap
    // Matches IDs -> Result string
    private final HashMap<String, String> searchBuffer = new HashMap<>();

    // Add search item to list
    public void addToBuffer(String id, String... words) {
        StringBuilder string = new StringBuilder();
        for (String word : words) {
            string.append(normalize(word)).append(" ");
        }

        searchBuffer.put(id, string.toString());
    }

    // Convert string to lowercase alphanumeric string, converting all characters to ASCII and turning double spaces into single ones
    // For example, é will be converted to e
    private String normalize(String string) {
        string = Normalizer.normalize(string, Normalizer.Form.NFD).replaceAll("[^\\p{ASCII}]", "");

        return string.toLowerCase()
                .replaceAll("(?!([a-z0-9 ])).*", "")
                .replaceAll("\\s\\s", " ");
    }

    // Execute search and return array with item id.
    public List<String> search(String query) {
        String[] words = normalize(query).split("\\s");

        if (words.length == 0) {
            return Collections.emptyList();
        }

        // Map for add all search result: id -> probability
        HashMap<String, Integer> results = new HashMap<>();
        for (String key : searchBuffer.keySet()) {
            int probability = 0;
            String value = searchBuffer.get(key);

            for (String word : words) {
                if (value.contains(word))
                    probability++;
            }

            if (probability > 0) {
                results.put(key, probability);
            }
        }


        // Filter by probability average, ie by how closely they match to our query
        // Ex: A = 10% B = 30% C = 70% (formula is (10+30+70)/3=36)
        // Afterwards remove all results with probability < 36
        int average = 0;
        for (String key : results.keySet()) {
            average += results.get(key);
        }
        average = average / Math.max(1, results.size());

        int i = 0;
        ArrayList<String> resultKeys = new ArrayList<>(Arrays.asList(results.keySet().toArray(new String[0])));
        while ((i < resultKeys.size() && resultKeys.size() > 1)) {
            if (results.get(resultKeys.get(i)) < average) {
                String key = resultKeys.get(i);
                resultKeys.remove(i);
                results.remove(key);
                i = 0;
                continue;
            }
            
            i++;
        }

        return Arrays.asList(results.keySet().toArray(new String[0]));
    }

    // Clear search buffer
    public void clearBuffer() {
        searchBuffer.clear();
    }
}
