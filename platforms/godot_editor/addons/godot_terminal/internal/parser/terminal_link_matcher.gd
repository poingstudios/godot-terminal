extends RefCounted

var _url_regex := RegEx.new()
var _res_path_regex := RegEx.new()
var _rel_path_regex := RegEx.new()

func _init() -> void:
	_url_regex.compile("https?://[a-zA-Z0-9.-]+(?:/[^\\s]*)?")
	_res_path_regex.compile("res://[^\\s:]+(?::(\\d+)(?::(\\d+))?)?")
	_rel_path_regex.compile("(?:[a-zA-Z0-9_./-]+\\.(?:gd|tscn|tres|c|cpp|h|hpp|json|md|txt|py|sh|cs)):(\\d+)(?::(\\d+))?")

func find_links(text: String) -> Array:
	var links: Array = []
	
	var url_matches := _url_regex.search_all(text)
	for m in url_matches:
		links.append({
			"type": "url",
			"target": m.get_string(),
			"start": m.get_start(),
			"end": m.get_end()
		})

	var res_matches := _res_path_regex.search_all(text)
	for m in res_matches:
		var line_num := 1
		var col_num := 1
		if m.get_group_count() >= 1 and not m.get_string(1).is_empty():
			line_num = int(m.get_string(1))
		if m.get_group_count() >= 2 and not m.get_string(2).is_empty():
			col_num = int(m.get_string(2))

		links.append({
			"type": "file",
			"target": m.get_string().split(":")[0],
			"line": line_num,
			"col": col_num,
			"start": m.get_start(),
			"end": m.get_end()
		})

	var rel_matches := _rel_path_regex.search_all(text)
	for m in rel_matches:
		var raw_str := m.get_string()
		var parts := raw_str.split(":")
		var path_part := parts[0]
		var line_num := int(parts[1]) if parts.size() > 1 else 1
		var col_num := int(parts[2]) if parts.size() > 2 else 1

		links.append({
			"type": "file",
			"target": "res://" + path_part.trim_prefix("./"),
			"line": line_num,
			"col": col_num,
			"start": m.get_start(),
			"end": m.get_end()
		})

	return links
