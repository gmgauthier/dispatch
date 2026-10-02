# Bug backlog

Reviewed 2026-10-01 against the 1.1.0 sources.

`meson test` runs `tests/test_mail_feed.cpp` (`mail-feed`) and `tests/test_trash.cpp` (`trash`). `trash` checks that `INBOX.Trash`, `INBOX.Sent`, and `[Gmail]/Drafts` fill those slots, that a second trash-like name does not become an extra folder, and that a server delete expunges only after a copy into a known trash mailbox. A failed UID EXPUNGE does not fall back to a mailbox-wide EXPUNGE. `mail-feed` checks simple address lists, `Re:` / `Fwd:` prefixes, an RFC 822 round trip, HTML-to-text, an ISO date, an RSS item, and an OPML round trip. The address test uses names without commas. The RFC 822 date check is a prefix, so it does not require the trailing space below. Launching Ephemeris goes through `Glib::shell_quote`. GMime encodes a CRLF in a subject, and libetpan dot-stuffs the SMTP body. Those are not defects.

## Open

### Sync writes the local Seen flag back and drops the server flag

- Severity: incorrect
- Confidence: high
- Where: `src/mail_imap.cpp:95`
- Trigger: Read a message, or mark it unread, in another client. Send/Recv in Dispatch.
- Outcome: `push_local_flags` STORE `+FLAGS \Seen` or `-FLAGS \Seen` for every local UID still on the server. The other client's read state is replaced by the filename flag in the local Maildir. Messages already on disk never import a server flag change.

### A comma inside a display name becomes a recipient

- Severity: incorrect
- Confidence: high
- Where: `src/mail_parse.cpp:303`, `src/mail_smtp.cpp:78`
- Trigger: To `Doe, Jane <jane@example.com>`, or pick an Ephemeris contact whose name contains a comma. Press Send.
- Outcome: `split_addresses` splits on every comma and semicolon, then takes the text inside `<...>`. The list is `Doe` and `jane@example.com`. SMTP issues `RCPT TO` for `Doe` first and, on failure, aborts the rest. The message stays in Outbox. `jane@example.com` is never tried when the first RCPT is rejected.

### A non-BMP numeric entity that survives XML parsing becomes invalid UTF-8

- Severity: incorrect
- Confidence: medium
- Where: `src/feed.cpp:320`
- Trigger: An RSS or Atom title whose entity is still literal text after libxml, for example `<title><![CDATA[Hello &#128512;]]></title>` or a double-escaped `&amp;#128512;`.
- Outcome: The encoder's comment says "UTF-8 BMP". Anything at or above U+0800, including U+1F600, is written as three bytes. U+1F600 becomes `FF 98 80` instead of `F0 9F 98 80`. The subject or feed name is not valid UTF-8. A normal `<title>Hello &#128512;</title>` is expanded by libxml before this function and is fine.

### Reply and Forward of an HTML-only message quote the entities

- Severity: incorrect
- Confidence: high
- Where: `src/mail_parse.cpp:91`, `src/mail_parse.cpp:226`
- Trigger: Open a message that has `text/html` and no `text/plain` part, whose body contains `Tom &amp; Jerry`. Reply or Forward.
- Outcome: The mail-side `html_to_text` strips tags and copies entity text unchanged. The quote contains `Tom &amp; Jerry`. A `multipart/alternative` that includes `text/plain` uses the plain part and does not hit this. The feed parser's `html_to_text` does decode entities. This copy does not.

### Unsubscribe during Refresh All writes the fetch into the wrong subscription

- Severity: data-loss
- Confidence: high
- Where: `src/main_window.cpp:1083`, `src/main_window.cpp:1013`
- Trigger: Two or more feeds. Refresh All. Unsubscribe a feed that sorts above one whose fetch has not finished.
- Outcome: `on_unsubscribe` erases the vector and does not rewrite `fetch_queue_` or `fetch_job_.replace_index`. `on_fetch_done` stores the parsed feed at the old index and `persist()` saves it. The subscription now living at that index is replaced, and its URL is dropped from the ini. If the index is past the new end, the feed is appended and the same URL is stored twice.

### Return in a bullet inserts the new bullet at the end of the line

- Severity: incorrect
- Confidence: high
- Where: `src/compose_format.cpp:133`
- Trigger: In compose, a line `• hello world` with the caret after `hello`. Press Return.
- Outcome: `paragraph_bounds` sets the insertion point to the end of the paragraph. The line stays `• hello world`, a new `• ` line is added after it, and `world` does not move with the caret. The key is consumed.

### A clicked mail link can open a local file

- Severity: security
- Confidence: high
- Where: `src/body_view.cpp:677`, `src/body_view.cpp:635`
- Trigger: Preview an HTML message that contains `<a href="file:///home/USER/.config/dispatch/dispatch.ini">` (or `file:///` plus any other local path) and click the link. Mail preview loads HTML with an empty base.
- Outcome: `resolve` returns an `http` or `https` URL unchanged, and returns any other URL unchanged when the base is empty. `open_uri` passes that string to `gtk_show_uri_on_window`. GTK opens the local file. The account password is in `dispatch.ini`. Image loads go through `http_get`, which refuses non-http(s). This is the click path.

### Send/Recv can select a different extra folder at the same index

- Severity: incorrect
- Confidence: high
- Where: `src/mail_store.cpp:379`, `src/main_window.cpp:1829`
- Trigger: View an extra mailbox (index at or past the built-in slots). Send/Recv while LIST returns a new mailbox whose display name sorts before that folder.
- Outcome: Built-in slots stay put. Extras are re-sorted on every sync. The UI restores the integer index, so the row now shows the newly inserted mailbox. Delete, Move, and the message list act on that folder.

### The folder list is mutated on the sync thread while the UI reads it

- Severity: crash
- Confidence: medium
- Where: `src/main_window.cpp:1804`, `src/mail_store.cpp:377`
- Trigger: Send/Recv, or the refresh timer, while clicking another folder or deleting mail. Only the Send/Recv button is disabled.
- Outcome: `sync_mailboxes` assigns folder fields, `push_back`s a newly discovered mailbox, and `std::sort`s the extra folders on the worker thread. `select_mail_folder` and `load_mail_folder` read `g_folders` on the UI thread. There is no lock around that vector. A reallocation or a sort overlapping a read is a data race and can crash.

### An RFC 822 date is shown with a trailing space

- Severity: incorrect
- Confidence: high
- Where: `src/feed.cpp:397`
- Trigger: `Fri, 25 Sep 2026 02:01:51 GMT`.
- Outcome: After the year token, the skip-spaces loop advances past the following space, and `substr` includes it. The visible date is `25 Sep 2026 ` with a trailing space. An ISO `YYYY-MM-DD` date returns ten characters and is fine.

## Closed

### A failed UID EXPUNGE expunges every deleted message in the mailbox

- Severity: data-loss
- Confidence: high
- Where: `src/mail_imap.cpp` `expunge_uids`
- Trigger: Send/Recv deletes or moves messages on a server where `UID EXPUNGE` fails (no UIDPLUS, or the command returns an error). Other messages in that mailbox are already `\Deleted` and are not in this UID set.
- Outcome: The fallback is mailbox-wide `EXPUNGE`. Those other messages are removed.
- Fixed in v1.1.3: UID EXPUNGE is the only expunge. When it fails, the other `\Deleted` messages stay. The UID set keeps the `\Deleted` flag from the store that succeeded.

### Delete expunges the server message when Trash is not an exact mailbox name

- Severity: data-loss
- Confidence: high
- Where: `src/mail_store.cpp` `role_for_imap`, `src/mail_imap.cpp` `move_uids_to_trash`
- Trigger: The account's trash mailbox is `INBOX.Trash`, `INBOX.Deleted`, or `[Gmail]/Trash`. Delete a message (status says it moved to Trash), then Send/Recv.
- Outcome: `role_for_imap` compares the full IMAP name with `trash`, `deleted`, `deleted messages`, `deleted items`, or `bin`. Those real names become extra folders, and the built-in Trash slot keeps an empty `imap`. `move_uids_to_trash` skips MOVE and COPY when the trash name is empty and calls `expunge_uids`, which stores `\Deleted` and expunges. The server copy is gone. The local file is only in the local Trash directory. The same expunge runs when MOVE and COPY both fail even if a trash name is set.
- Fixed in v1.1.2: The leaf name fills the Trash, Sent, or Drafts slot, so `INBOX.Trash` and `[Gmail]/Trash` are the trash mailbox. A delete with no trash mailbox, or a MOVE and COPY that both fail, leaves the server message. A successful COPY still expunges the source.
