# Build-time UI text. Override a key with -DONLYC_STRING_<KEY>=<text>.
# Format resources retain their printf conversion types and argument order.

onlyc_resource_string(BRAND_NAME VALUE "#34049")
include("${CMAKE_CURRENT_LIST_DIR}/../view/Comments.cmake")
onlyc_resource_string(COMMENTS_LABEL VALUE "Reactions and comments")
onlyc_resource_string(BRAND_SUFFIX VALUE "/ Translations")

onlyc_resource_string(SITE_NAME VALUE "#34049 Translations")
onlyc_resource_string(SITE_DESCRIPTION VALUE "Song translations by #34049.")

onlyc_resource_string(HOME VALUE "home")

onlyc_resource_string(TITLE_SEPARATOR VALUE " · ")

onlyc_resource_string(LOADING VALUE "Loading…")

onlyc_resource_string(
    JAVASCRIPT_REQUIRED
    VALUE
    "This reader needs JavaScript enabled to load its WebAssembly application."
)

onlyc_resource_string(CATALOG_TITLE VALUE "Songs")
onlyc_resource_string(CATALOG_SEARCH VALUE "Find a song")
onlyc_resource_string(CATALOG_BROWSE_LABEL VALUE "Browse songs by letter")
onlyc_resource_string(CATALOG_EMPTY VALUE "No songs found.")
onlyc_resource_string(CATALOG_SONG_SINGULAR VALUE "song")
onlyc_resource_string(CATALOG_SONG_PLURAL VALUE "songs")
onlyc_resource_string(CATALOG_COUNT_FORMAT VALUE "%zu %s")

onlyc_resource_string(READER_NOT_FOUND VALUE "Song not found")
onlyc_resource_string(READER_BACK VALUE "← All songs")
onlyc_resource_string(READER_LYRICS VALUE "Lyrics")
onlyc_resource_string(READER_CREDIT VALUE "Translator: ")
onlyc_resource_string(READER_EXTERNAL_MARK VALUE " ↗")
onlyc_resource_string(READER_ANONYMOUS VALUE "Anonymous")
onlyc_resource_string(READER_TRANSLATED_LABEL VALUE "Translated lyrics")
onlyc_resource_string(READER_CLOSE_LABEL VALUE "Close annotation")
onlyc_resource_string(READER_CLOSE VALUE "Close ×")
onlyc_resource_string(READER_EMPTY_LINE_FORMAT VALUE "%s, line %zu (empty)")

onlyc_resource_string(EDITOR_PREVIEW VALUE "Preview")
onlyc_resource_string(EDITOR_EDIT_DRAFT VALUE "Edit draft")
onlyc_resource_string(EDITOR_RESUME_DRAFT VALUE "Resume draft")
onlyc_resource_string(EDITOR_METADATA_LABEL VALUE "Song metadata")
onlyc_resource_string(EDITOR_SAVE_HINT VALUE "Export to save your changes.")
onlyc_resource_string(EDITOR_IMPORT VALUE "Import YAML")
onlyc_resource_string(EDITOR_EXPORT VALUE "↓ Export YAML")
onlyc_resource_string(EDITOR_DETAILS VALUE "Song details & links")
onlyc_resource_string(EDITOR_TITLE VALUE "Original title")
onlyc_resource_string(EDITOR_ARTIST VALUE "Artist")
onlyc_resource_string(EDITOR_TRANSLATOR VALUE "Translator")
onlyc_resource_string(EDITOR_SLUG VALUE "URL slug")
onlyc_resource_string(EDITOR_ARTWORK VALUE "Artwork URL")
onlyc_resource_string(EDITOR_COLOR VALUE "Artwork color · #RRGGBB (detected when empty)")
onlyc_resource_string(EDITOR_SECTION_LABEL VALUE "Section label")
onlyc_resource_string(EDITOR_SECTION_LYRICS VALUE "Edit section lyrics")
onlyc_resource_string(EDITOR_ORIGINAL VALUE "Original · one line per translated line")
onlyc_resource_string(EDITOR_TRANSLATED VALUE "Translation")
onlyc_resource_string(EDITOR_RANGE_START VALUE "From line")
onlyc_resource_string(EDITOR_RANGE_END VALUE "Through line")
onlyc_resource_string(EDITOR_ANNOTATION VALUE "Annotation · Markdown")
onlyc_resource_string(EDITOR_APPLY VALUE "Apply changes")
onlyc_resource_string(EDITOR_REMOVE_ANNOTATION VALUE "Detach annotation")
onlyc_resource_string(EDITOR_DELETE_SECTION VALUE "Delete section")
onlyc_resource_string(EDITOR_ADD_SECTION VALUE "+ Add a section")
onlyc_resource_string(EDITOR_NEW_SECTION VALUE "New section")

onlyc_resource_string(
    CONFIRM_REPLACE_DRAFT
    VALUE
    "Replace the unexported draft with the imported file?"
)
onlyc_resource_string(
    CONFIRM_DELETE_SECTION
    VALUE
    "Delete this section? Shared annotations will be kept."
)

onlyc_resource_string(STATUS_APPLIED VALUE "Changes applied. Export YAML to save.")
onlyc_resource_string(STATUS_UNEXPORTED VALUE "Unexported changes.")
onlyc_resource_string(STATUS_IMPORTED VALUE "YAML imported.")
onlyc_resource_string(STATUS_DOWNLOADING VALUE "YAML download started.")

onlyc_resource_string(
    ERROR_ANNOTATION_RANGE
    VALUE
    "Annotation start and end must be whole line numbers from 1 to 256."
)
onlyc_resource_string(ERROR_MAX_ANNOTATIONS VALUE "At most 128 annotations are supported.")
onlyc_resource_string(ERROR_MAX_SECTIONS VALUE "At most 32 sections are supported.")
onlyc_resource_string(ERROR_KEEP_SECTION VALUE "Keep at least one section in the song.")
onlyc_resource_string(ERROR_OPEN_EDITOR VALUE "Not enough memory to open the editor.")
onlyc_resource_string(ERROR_SAVE VALUE "Not enough memory to save changes.")
onlyc_resource_string(
    ERROR_INPUT_BYTE_LIMIT
    VALUE
    "A field exceeds its UTF-8 byte limit. Shorten the text before continuing."
)
onlyc_resource_string(ERROR_IMPORT_MEMORY VALUE "Not enough memory to import.")
onlyc_resource_string(ERROR_DOWNLOAD VALUE "Unable to start the YAML download.")
onlyc_resource_string(ERROR_EXPORT VALUE "Unable to export this song.")
onlyc_resource_string(ERROR_KEEP_EDIT VALUE "Not enough memory to keep this edit.")
onlyc_resource_string(ERROR_READ_FILE VALUE "Unable to read the selected file.")
onlyc_resource_string(ERROR_ONE_FILE VALUE "Drop one YAML file at a time.")
onlyc_resource_string(ERROR_YAML_EXTENSION VALUE "Choose a .yaml or .yml file.")
onlyc_resource_string(ERROR_IMPORT_SIZE VALUE "Choose a nonempty YAML file no larger than 1 MiB.")
onlyc_resource_string(ERROR_OPEN_SONG VALUE "Not enough memory to open this song.")

onlyc_resource_string(EDITOR_TITLE_TRANSLATED VALUE "Translated title")
onlyc_resource_string(EDITOR_TITLE_ANNOTATION VALUE "Title annotation key")
onlyc_resource_string(EDITOR_ANNOTATION_REF VALUE "Annotation key · reuse a key to share its note")
onlyc_resource_string(EDITOR_LINK_LABEL VALUE "Link label")
onlyc_resource_string(EDITOR_LINK_URL VALUE "Link URL")
onlyc_resource_string(EDITOR_ADD_LINK VALUE "Add link")
onlyc_resource_string(EDITOR_REMOVE_LINK VALUE "Remove link")
onlyc_resource_string(ERROR_MAX_LINKS VALUE "At most 32 links are supported.")
onlyc_resource_string(ERROR_MAX_RANGES VALUE "At most 256 annotation ranges are supported.")
