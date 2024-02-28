package com.panda3ds.pandroid.view.code.syntax;

import java.util.regex.Pattern;

class PatternUtils {
	public static Pattern buildGenericKeywords(String... keywords) {
		StringBuilder builder = new StringBuilder();
		builder.append("\\b(");
		for (int i = 0; i < keywords.length; i++) {
			builder.append(keywords[i]);
			if (i + 1 != keywords.length) {
				builder.append("|");
			}
		}

		builder.append(")\\b");
		return Pattern.compile(builder.toString());
	}
}
