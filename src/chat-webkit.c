/***************************************************************************
 *   Copyright (C) 2011 by levin                                           *
 *   levin108@gmail.com                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.            *
 ***************************************************************************/
#include "config.h"
#include "conv.h"
#include "chat-webkit.h"

#ifdef USE_WEBKIT
 #include <webkit/webkit.h>
#endif

static HybridChatTextOps webkit_ops = {
	hybrid_chat_webkit_create,
	hybrid_chat_webkit_append,
	hybrid_chat_webkit_notify
};

struct timeout_data {
	GtkWidget *webkit;
	gchar *script;
};

void 
hybrid_chat_set_webkit_ops(void)
{
	hybrid_conv_set_chat_text_ops(&webkit_ops);
}

static gchar*
escape_string(const gchar *str)
{
	GString *res;

	res = g_string_sized_new(strlen(str));

	while (str && *str) {
		switch (*str) {
			case 13:
				res = g_string_append(res, "<br/>");
				break;
			case '\"':
				res = g_string_append(res, "\\\"");
				break;
			case '\t':
				break;
			default:
				res = g_string_append_c(res, *str);
				break;
		}
		str ++;
	}

	return g_string_free(res, FALSE);
}

static gchar*
escape_html(const gchar *str)
{
	GString *res;

	res = g_string_sized_new(strlen(str));

	while (str && *str != '\0') {
		switch (*str) {
			case '\n':
			case '\t':
				break;
			default:
				res = g_string_append_c(res, *str);
				break;
		}
		str ++;
	}

	return g_string_free(res, FALSE);
}

GtkWidget*
hybrid_chat_webkit_create(void)
{
#ifdef USE_WEBKIT
	GtkWidget *webkit = webkit_web_view_new();
	gchar *css_string;
	gchar *full_html;
	gchar *template_html;

	if (!g_file_get_contents(UI_DIR"template.html", &template_html,
				NULL, NULL)) {
		return NULL;
	}

	if (!g_file_get_contents(UI_DIR"default.css", &css_string, NULL, NULL)) {
		g_free(template_html);
		return NULL;
	}

	full_html = g_strdup_printf(template_html, css_string);
	g_free(css_string);

	printf("%s\n", full_html);

	webkit_web_view_load_string(WEBKIT_WEB_VIEW(webkit), full_html, NULL, NULL, "file://"UI_DIR);

	g_free(template_html);
	g_free(full_html);

	return webkit;
#else
	return NULL;
#endif
}

gboolean
timeout_cb(struct timeout_data *data)
{
	WebKitLoadStatus status;

	g_object_get(data->webkit, "load-status", &status, NULL);

	if (status != WEBKIT_LOAD_FINISHED) {
		return TRUE;
	}

	webkit_web_view_execute_script(WEBKIT_WEB_VIEW(data->webkit), data->script);
	g_free(data->script);
	g_free(data);


	return FALSE;
}

void
hybrid_chat_webkit_append(GtkWidget *textview, HybridAccount *account,
							HybridBuddy *buddy,	const gchar *message, time_t msg_time)
{
	gchar *format;
	gchar *html, *escaped_html, *escaped_message;
	gchar *script;
	WebKitLoadStatus status;
	struct timeout_data *data;
	const gchar *content_html;
	gchar *icon_path;
	gchar *icon_name;

	if (buddy) {
		content_html = UI_DIR"recv.html";

	} else {
		content_html = UI_DIR"send.html";
	}

	escaped_message = escape_string(message);

	if (!g_file_get_contents(content_html, &format, NULL, NULL)) {
		g_free(escaped_message);
		return;
	}

	icon_path = hybrid_config_get_path();

	if (buddy) {
		icon_name = g_strdup_printf("file://%s/icons/%s", icon_path, buddy->icon_name);
		html = g_strdup_printf(format, 
				icon_name,
				buddy->name && *buddy->name ? buddy->name : buddy->id,
				escaped_message);
	} else {
		icon_name = g_strdup_printf("file://%s/icons/%s", icon_path, account->icon_name);
		html = g_strdup_printf(format, 
				account->nickname && *account->nickname ? 
				account->nickname : account->username,
				escaped_message,
				icon_name
				);
	}

	g_free(icon_path);
	g_free(icon_name);


	escaped_html = escape_html(html);

	g_object_get(textview, "load-status", &status, NULL);

	script = g_strdup_printf("appendMessage(\"%s\")", escaped_html);

	if (WEBKIT_LOAD_FINISHED != status && WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT != status) {
		data = g_new0(struct timeout_data, 1);
		data->webkit = textview;
		data->script = script;
		g_timeout_add_seconds(0, (GSourceFunc)timeout_cb, data);

	} else {
		webkit_web_view_execute_script(WEBKIT_WEB_VIEW(textview), script);
		g_free(script);
	}

	g_free(escaped_html);
	g_free(escaped_message);
	g_free(html);
	g_free(format);
	return;
}

void
hybrid_chat_webkit_notify(GtkWidget *textview, const gchar *text, gint type)
{
	return;
}
