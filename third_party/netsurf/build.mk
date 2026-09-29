# СГЕНЕРИРОВАНО (перенос NetSurf в MyOS): правила сборки NetSurf и
# его библиотек. Флаги - как у их собственной сборки.

NS_INC := -I$(TP)/freetype/myos/include -I$(TP)/libjpeg-turbo/myos -I$(TP)/curl/include -I$(TP)/freetype/include -I$(TP)/libjpeg-turbo/src -I$(TP)/libpng -I$(TP)/netsurf/include -I$(TP)/netsurf/libcss/include -I$(TP)/netsurf/libdom/include -I$(TP)/netsurf/libhubbub/include -I$(TP)/netsurf/libnsbmp/include -I$(TP)/netsurf/libnsfb/include -I$(TP)/netsurf/libnsgif/include -I$(TP)/netsurf/libnslog/include -I$(TP)/netsurf/libnspsl/include -I$(TP)/netsurf/libnsutils/include -I$(TP)/netsurf/libparserutils/include -I$(TP)/netsurf/libwapcaplet/include -I$(TP)/utf8proc -I$(TP)/zlib

NS_OBJS_libwapcaplet := $(NS_OBJ)/libwapcaplet/src_libwapcaplet.o
NS_OBJS_libparserutils := $(NS_OBJ)/libparserutils/src_charset_aliases.o $(NS_OBJ)/libparserutils/src_charset_codec.o $(NS_OBJ)/libparserutils/src_charset_codecs_codec_ascii.o $(NS_OBJ)/libparserutils/src_charset_codecs_codec_8859.o $(NS_OBJ)/libparserutils/src_charset_codecs_codec_ext8.o $(NS_OBJ)/libparserutils/src_charset_codecs_codec_utf8.o $(NS_OBJ)/libparserutils/src_charset_codecs_codec_utf16.o $(NS_OBJ)/libparserutils/src_charset_encodings_utf8.o $(NS_OBJ)/libparserutils/src_charset_encodings_utf16.o $(NS_OBJ)/libparserutils/src_input_filter.o $(NS_OBJ)/libparserutils/src_input_inputstream.o $(NS_OBJ)/libparserutils/src_utils_buffer.o $(NS_OBJ)/libparserutils/src_utils_errors.o $(NS_OBJ)/libparserutils/src_utils_stack.o $(NS_OBJ)/libparserutils/src_utils_vector.o
NS_OBJS_libhubbub := $(NS_OBJ)/libhubbub/src_parser.o $(NS_OBJ)/libhubbub/src_charset_detect.o $(NS_OBJ)/libhubbub/src_tokeniser_entities.o $(NS_OBJ)/libhubbub/src_tokeniser_tokeniser.o $(NS_OBJ)/libhubbub/src_treebuilder_treebuilder.o $(NS_OBJ)/libhubbub/src_treebuilder_initial.o $(NS_OBJ)/libhubbub/src_treebuilder_before_html.o $(NS_OBJ)/libhubbub/src_treebuilder_before_head.o $(NS_OBJ)/libhubbub/src_treebuilder_in_head.o $(NS_OBJ)/libhubbub/src_treebuilder_in_head_noscript.o $(NS_OBJ)/libhubbub/src_treebuilder_after_head.o $(NS_OBJ)/libhubbub/src_treebuilder_in_body.o $(NS_OBJ)/libhubbub/src_treebuilder_in_table.o $(NS_OBJ)/libhubbub/src_treebuilder_in_caption.o $(NS_OBJ)/libhubbub/src_treebuilder_in_column_group.o $(NS_OBJ)/libhubbub/src_treebuilder_in_table_body.o $(NS_OBJ)/libhubbub/src_treebuilder_in_row.o $(NS_OBJ)/libhubbub/src_treebuilder_in_cell.o $(NS_OBJ)/libhubbub/src_treebuilder_in_select.o $(NS_OBJ)/libhubbub/src_treebuilder_in_select_in_table.o $(NS_OBJ)/libhubbub/src_treebuilder_in_foreign_content.o $(NS_OBJ)/libhubbub/src_treebuilder_after_body.o $(NS_OBJ)/libhubbub/src_treebuilder_in_frameset.o $(NS_OBJ)/libhubbub/src_treebuilder_after_frameset.o $(NS_OBJ)/libhubbub/src_treebuilder_after_after_body.o $(NS_OBJ)/libhubbub/src_treebuilder_after_after_frameset.o $(NS_OBJ)/libhubbub/src_treebuilder_generic_rcdata.o $(NS_OBJ)/libhubbub/src_treebuilder_element-type.o $(NS_OBJ)/libhubbub/src_utils_errors.o $(NS_OBJ)/libhubbub/src_utils_string.o
NS_OBJS_libdom := $(NS_OBJ)/libdom/bindings_hubbub_parser.o $(NS_OBJ)/libdom/src_core_string.o $(NS_OBJ)/libdom/src_core_node.o $(NS_OBJ)/libdom/src_core_attr.o $(NS_OBJ)/libdom/src_core_characterdata.o $(NS_OBJ)/libdom/src_core_element.o $(NS_OBJ)/libdom/src_core_implementation.o $(NS_OBJ)/libdom/src_core_text.o $(NS_OBJ)/libdom/src_core_typeinfo.o $(NS_OBJ)/libdom/src_core_comment.o $(NS_OBJ)/libdom/src_core_namednodemap.o $(NS_OBJ)/libdom/src_core_nodelist.o $(NS_OBJ)/libdom/src_core_cdatasection.o $(NS_OBJ)/libdom/src_core_document_type.o $(NS_OBJ)/libdom/src_core_entity_ref.o $(NS_OBJ)/libdom/src_core_pi.o $(NS_OBJ)/libdom/src_core_doc_fragment.o $(NS_OBJ)/libdom/src_core_document.o $(NS_OBJ)/libdom/src_core_tokenlist.o $(NS_OBJ)/libdom/src_events_event.o $(NS_OBJ)/libdom/src_events_dispatch.o $(NS_OBJ)/libdom/src_events_event_target.o $(NS_OBJ)/libdom/src_events_document_event.o $(NS_OBJ)/libdom/src_events_custom_event.o $(NS_OBJ)/libdom/src_events_keyboard_event.o $(NS_OBJ)/libdom/src_events_mouse_wheel_event.o $(NS_OBJ)/libdom/src_events_text_event.o $(NS_OBJ)/libdom/src_events_event_listener.o $(NS_OBJ)/libdom/src_events_mouse_event.o $(NS_OBJ)/libdom/src_events_mutation_event.o $(NS_OBJ)/libdom/src_events_ui_event.o $(NS_OBJ)/libdom/src_events_mouse_multi_wheel_event.o $(NS_OBJ)/libdom/src_events_mutation_name_event.o $(NS_OBJ)/libdom/src_html_html_applet_element.o $(NS_OBJ)/libdom/src_html_html_area_element.o $(NS_OBJ)/libdom/src_html_html_anchor_element.o $(NS_OBJ)/libdom/src_html_html_basefont_element.o $(NS_OBJ)/libdom/src_html_html_base_element.o $(NS_OBJ)/libdom/src_html_html_body_element.o $(NS_OBJ)/libdom/src_html_html_button_element.o $(NS_OBJ)/libdom/src_html_html_canvas_element.o $(NS_OBJ)/libdom/src_html_html_collection.o $(NS_OBJ)/libdom/src_html_html_document.o $(NS_OBJ)/libdom/src_html_html_element.o $(NS_OBJ)/libdom/src_html_html_dlist_element.o $(NS_OBJ)/libdom/src_html_html_directory_element.o $(NS_OBJ)/libdom/src_html_html_options_collection.o $(NS_OBJ)/libdom/src_html_html_html_element.o $(NS_OBJ)/libdom/src_html_html_head_element.o $(NS_OBJ)/libdom/src_html_html_link_element.o $(NS_OBJ)/libdom/src_html_html_title_element.o $(NS_OBJ)/libdom/src_html_html_meta_element.o $(NS_OBJ)/libdom/src_html_html_style_element.o $(NS_OBJ)/libdom/src_html_html_form_element.o $(NS_OBJ)/libdom/src_html_html_select_element.o $(NS_OBJ)/libdom/src_html_html_input_element.o $(NS_OBJ)/libdom/src_html_html_text_area_element.o $(NS_OBJ)/libdom/src_html_html_opt_group_element.o $(NS_OBJ)/libdom/src_html_html_option_element.o $(NS_OBJ)/libdom/src_html_html_hr_element.o $(NS_OBJ)/libdom/src_html_html_menu_element.o $(NS_OBJ)/libdom/src_html_html_fieldset_element.o $(NS_OBJ)/libdom/src_html_html_legend_element.o $(NS_OBJ)/libdom/src_html_html_div_element.o $(NS_OBJ)/libdom/src_html_html_paragraph_element.o $(NS_OBJ)/libdom/src_html_html_heading_element.o $(NS_OBJ)/libdom/src_html_html_quote_element.o $(NS_OBJ)/libdom/src_html_html_pre_element.o $(NS_OBJ)/libdom/src_html_html_br_element.o $(NS_OBJ)/libdom/src_html_html_label_element.o $(NS_OBJ)/libdom/src_html_html_ulist_element.o $(NS_OBJ)/libdom/src_html_html_olist_element.o $(NS_OBJ)/libdom/src_html_html_li_element.o $(NS_OBJ)/libdom/src_html_html_font_element.o $(NS_OBJ)/libdom/src_html_html_mod_element.o $(NS_OBJ)/libdom/src_html_html_image_element.o $(NS_OBJ)/libdom/src_html_html_object_element.o $(NS_OBJ)/libdom/src_html_html_param_element.o $(NS_OBJ)/libdom/src_html_html_map_element.o $(NS_OBJ)/libdom/src_html_html_script_element.o $(NS_OBJ)/libdom/src_html_html_tablecaption_element.o $(NS_OBJ)/libdom/src_html_html_tablecell_element.o $(NS_OBJ)/libdom/src_html_html_tablecol_element.o $(NS_OBJ)/libdom/src_html_html_tablesection_element.o $(NS_OBJ)/libdom/src_html_html_table_element.o $(NS_OBJ)/libdom/src_html_html_tablerow_element.o $(NS_OBJ)/libdom/src_html_html_frameset_element.o $(NS_OBJ)/libdom/src_html_html_frame_element.o $(NS_OBJ)/libdom/src_html_html_iframe_element.o $(NS_OBJ)/libdom/src_html_html_isindex_element.o $(NS_OBJ)/libdom/src_utils_namespace.o $(NS_OBJ)/libdom/src_utils_hashtable.o $(NS_OBJ)/libdom/src_utils_character_valid.o $(NS_OBJ)/libdom/src_utils_validate.o $(NS_OBJ)/libdom/src_utils_walk.o
NS_OBJS_libcss := $(NS_OBJ)/libcss/src_stylesheet.o $(NS_OBJ)/libcss/src_charset_detect.o $(NS_OBJ)/libcss/src_lex_lex.o $(NS_OBJ)/libcss/src_parse_parse.o $(NS_OBJ)/libcss/src_parse_language.o $(NS_OBJ)/libcss/src_parse_important.o $(NS_OBJ)/libcss/src_parse_propstrings.o $(NS_OBJ)/libcss/src_parse_font_face.o $(NS_OBJ)/libcss/src_parse_mq.o $(NS_OBJ)/libcss/src_parse_properties_azimuth.o $(NS_OBJ)/libcss/src_parse_properties_background.o $(NS_OBJ)/libcss/src_parse_properties_background_position.o $(NS_OBJ)/libcss/src_parse_properties_border.o $(NS_OBJ)/libcss/src_parse_properties_border_color.o $(NS_OBJ)/libcss/src_parse_properties_border_spacing.o $(NS_OBJ)/libcss/src_parse_properties_border_style.o $(NS_OBJ)/libcss/src_parse_properties_border_width.o $(NS_OBJ)/libcss/src_parse_properties_clip.o $(NS_OBJ)/libcss/src_parse_properties_columns.o $(NS_OBJ)/libcss/src_parse_properties_column_rule.o $(NS_OBJ)/libcss/src_parse_properties_content.o $(NS_OBJ)/libcss/src_parse_properties_cue.o $(NS_OBJ)/libcss/src_parse_properties_cursor.o $(NS_OBJ)/libcss/src_parse_properties_elevation.o $(NS_OBJ)/libcss/src_parse_properties_fill_opacity.o $(NS_OBJ)/libcss/src_parse_properties_flex.o $(NS_OBJ)/libcss/src_parse_properties_flex_flow.o $(NS_OBJ)/libcss/src_parse_properties_font.o $(NS_OBJ)/libcss/src_parse_properties_font_family.o $(NS_OBJ)/libcss/src_parse_properties_font_weight.o $(NS_OBJ)/libcss/src_parse_properties_list_style.o $(NS_OBJ)/libcss/src_parse_properties_list_style_type.o $(NS_OBJ)/libcss/src_parse_properties_margin.o $(NS_OBJ)/libcss/src_parse_properties_opacity.o $(NS_OBJ)/libcss/src_parse_properties_outline.o $(NS_OBJ)/libcss/src_parse_properties_overflow.o $(NS_OBJ)/libcss/src_parse_properties_padding.o $(NS_OBJ)/libcss/src_parse_properties_pause.o $(NS_OBJ)/libcss/src_parse_properties_play_during.o $(NS_OBJ)/libcss/src_parse_properties_properties.o $(NS_OBJ)/libcss/src_parse_properties_quotes.o $(NS_OBJ)/libcss/src_parse_properties_stroke_opacity.o $(NS_OBJ)/libcss/src_parse_properties_text_decoration.o $(NS_OBJ)/libcss/src_parse_properties_utils.o $(NS_OBJ)/libcss/src_parse_properties_voice_family.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_background_repeat.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_collapse.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_cue_after.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_cue_before.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_direction.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_display.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_empty_cells.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_float.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_font_size.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_font_style.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_font_variant.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_height.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_letter_spacing.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_line_height.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_top.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_bottom.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_left.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_right.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_max_height.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_max_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_min_height.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_min_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_color.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_padding_side.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_padding_bottom.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_padding_left.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_padding_top.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_padding_right.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_margin_side.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_margin_top.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_margin_bottom.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_margin_left.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_margin_right.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_side.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_top.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_bottom.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_left.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_right.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_side_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_top_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_bottom_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_left_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_right_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_side_style.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_top_style.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_bottom_style.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_left_style.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_right_style.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_side_color.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_top_color.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_bottom_color.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_left_color.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_right_color.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_counter_increment.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_counter_reset.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_background_attachment.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_background_color.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_caption_side.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_clear.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_background_image.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_list_style_image.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_list_style_position.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_orphans.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_outline_color.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_outline_style.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_outline_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_overflow_x.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_overflow_y.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_page_break_after.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_page_break_before.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_page_break_inside.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_pause_after.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_pause_before.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_pitch.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_pitch_range.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_position.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_richness.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_speak.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_speak_header.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_speak_numeral.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_speak_punctuation.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_speech_rate.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_stress.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_table_layout.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_text_align.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_text_indent.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_text_transform.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_unicode_bidi.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_vertical_align.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_visibility.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_volume.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_white_space.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_widows.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_word_spacing.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_z_index.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_break_after.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_break_before.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_break_inside.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_count.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_fill.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_gap.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_rule_color.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_rule_style.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_rule_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_span.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_width.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_writing_mode.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_box_sizing.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_align_content.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_align_items.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_align_self.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_flex_basis.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_flex_direction.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_flex_grow.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_flex_shrink.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_flex_wrap.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_justify_content.o $(NS_OBJ)/libcss/src_parse_properties_autogenerated_order.o $(NS_OBJ)/libcss/src_select_arena.o $(NS_OBJ)/libcss/src_select_calc.o $(NS_OBJ)/libcss/src_select_computed.o $(NS_OBJ)/libcss/src_select_dispatch.o $(NS_OBJ)/libcss/src_select_hash.o $(NS_OBJ)/libcss/src_select_select.o $(NS_OBJ)/libcss/src_select_strings.o $(NS_OBJ)/libcss/src_select_font_face.o $(NS_OBJ)/libcss/src_select_format_list_style.o $(NS_OBJ)/libcss/src_select_unit.o $(NS_OBJ)/libcss/src_select_properties_helpers.o $(NS_OBJ)/libcss/src_select_properties_align_content.o $(NS_OBJ)/libcss/src_select_properties_align_items.o $(NS_OBJ)/libcss/src_select_properties_align_self.o $(NS_OBJ)/libcss/src_select_properties_azimuth.o $(NS_OBJ)/libcss/src_select_properties_background_attachment.o $(NS_OBJ)/libcss/src_select_properties_background_color.o $(NS_OBJ)/libcss/src_select_properties_background_image.o $(NS_OBJ)/libcss/src_select_properties_background_position.o $(NS_OBJ)/libcss/src_select_properties_background_repeat.o $(NS_OBJ)/libcss/src_select_properties_border_bottom_color.o $(NS_OBJ)/libcss/src_select_properties_border_bottom_style.o $(NS_OBJ)/libcss/src_select_properties_border_bottom_width.o $(NS_OBJ)/libcss/src_select_properties_border_collapse.o $(NS_OBJ)/libcss/src_select_properties_border_left_color.o $(NS_OBJ)/libcss/src_select_properties_border_left_style.o $(NS_OBJ)/libcss/src_select_properties_border_left_width.o $(NS_OBJ)/libcss/src_select_properties_border_right_color.o $(NS_OBJ)/libcss/src_select_properties_border_right_style.o $(NS_OBJ)/libcss/src_select_properties_border_right_width.o $(NS_OBJ)/libcss/src_select_properties_border_spacing.o $(NS_OBJ)/libcss/src_select_properties_border_top_color.o $(NS_OBJ)/libcss/src_select_properties_border_top_style.o $(NS_OBJ)/libcss/src_select_properties_border_top_width.o $(NS_OBJ)/libcss/src_select_properties_bottom.o $(NS_OBJ)/libcss/src_select_properties_box_sizing.o $(NS_OBJ)/libcss/src_select_properties_break_after.o $(NS_OBJ)/libcss/src_select_properties_break_before.o $(NS_OBJ)/libcss/src_select_properties_break_inside.o $(NS_OBJ)/libcss/src_select_properties_caption_side.o $(NS_OBJ)/libcss/src_select_properties_clear.o $(NS_OBJ)/libcss/src_select_properties_clip.o $(NS_OBJ)/libcss/src_select_properties_color.o $(NS_OBJ)/libcss/src_select_properties_column_count.o $(NS_OBJ)/libcss/src_select_properties_column_fill.o $(NS_OBJ)/libcss/src_select_properties_column_gap.o $(NS_OBJ)/libcss/src_select_properties_column_rule_color.o $(NS_OBJ)/libcss/src_select_properties_column_rule_style.o $(NS_OBJ)/libcss/src_select_properties_column_rule_width.o $(NS_OBJ)/libcss/src_select_properties_column_span.o $(NS_OBJ)/libcss/src_select_properties_column_width.o $(NS_OBJ)/libcss/src_select_properties_content.o $(NS_OBJ)/libcss/src_select_properties_counter_increment.o $(NS_OBJ)/libcss/src_select_properties_counter_reset.o $(NS_OBJ)/libcss/src_select_properties_cue_after.o $(NS_OBJ)/libcss/src_select_properties_cue_before.o $(NS_OBJ)/libcss/src_select_properties_cursor.o $(NS_OBJ)/libcss/src_select_properties_direction.o $(NS_OBJ)/libcss/src_select_properties_display.o $(NS_OBJ)/libcss/src_select_properties_elevation.o $(NS_OBJ)/libcss/src_select_properties_empty_cells.o $(NS_OBJ)/libcss/src_select_properties_fill_opacity.o $(NS_OBJ)/libcss/src_select_properties_flex_basis.o $(NS_OBJ)/libcss/src_select_properties_flex_direction.o $(NS_OBJ)/libcss/src_select_properties_flex_grow.o $(NS_OBJ)/libcss/src_select_properties_flex_shrink.o $(NS_OBJ)/libcss/src_select_properties_flex_wrap.o $(NS_OBJ)/libcss/src_select_properties_float.o $(NS_OBJ)/libcss/src_select_properties_font_family.o $(NS_OBJ)/libcss/src_select_properties_font_size.o $(NS_OBJ)/libcss/src_select_properties_font_style.o $(NS_OBJ)/libcss/src_select_properties_font_variant.o $(NS_OBJ)/libcss/src_select_properties_font_weight.o $(NS_OBJ)/libcss/src_select_properties_height.o $(NS_OBJ)/libcss/src_select_properties_justify_content.o $(NS_OBJ)/libcss/src_select_properties_left.o $(NS_OBJ)/libcss/src_select_properties_letter_spacing.o $(NS_OBJ)/libcss/src_select_properties_line_height.o $(NS_OBJ)/libcss/src_select_properties_list_style_image.o $(NS_OBJ)/libcss/src_select_properties_list_style_position.o $(NS_OBJ)/libcss/src_select_properties_list_style_type.o $(NS_OBJ)/libcss/src_select_properties_margin_bottom.o $(NS_OBJ)/libcss/src_select_properties_margin_left.o $(NS_OBJ)/libcss/src_select_properties_margin_right.o $(NS_OBJ)/libcss/src_select_properties_margin_top.o $(NS_OBJ)/libcss/src_select_properties_max_height.o $(NS_OBJ)/libcss/src_select_properties_max_width.o $(NS_OBJ)/libcss/src_select_properties_min_height.o $(NS_OBJ)/libcss/src_select_properties_min_width.o $(NS_OBJ)/libcss/src_select_properties_opacity.o $(NS_OBJ)/libcss/src_select_properties_order.o $(NS_OBJ)/libcss/src_select_properties_orphans.o $(NS_OBJ)/libcss/src_select_properties_outline_color.o $(NS_OBJ)/libcss/src_select_properties_outline_style.o $(NS_OBJ)/libcss/src_select_properties_outline_width.o $(NS_OBJ)/libcss/src_select_properties_overflow_x.o $(NS_OBJ)/libcss/src_select_properties_overflow_y.o $(NS_OBJ)/libcss/src_select_properties_padding_bottom.o $(NS_OBJ)/libcss/src_select_properties_padding_left.o $(NS_OBJ)/libcss/src_select_properties_padding_right.o $(NS_OBJ)/libcss/src_select_properties_padding_top.o $(NS_OBJ)/libcss/src_select_properties_page_break_after.o $(NS_OBJ)/libcss/src_select_properties_page_break_before.o $(NS_OBJ)/libcss/src_select_properties_page_break_inside.o $(NS_OBJ)/libcss/src_select_properties_pause_after.o $(NS_OBJ)/libcss/src_select_properties_pause_before.o $(NS_OBJ)/libcss/src_select_properties_pitch.o $(NS_OBJ)/libcss/src_select_properties_pitch_range.o $(NS_OBJ)/libcss/src_select_properties_play_during.o $(NS_OBJ)/libcss/src_select_properties_position.o $(NS_OBJ)/libcss/src_select_properties_quotes.o $(NS_OBJ)/libcss/src_select_properties_richness.o $(NS_OBJ)/libcss/src_select_properties_right.o $(NS_OBJ)/libcss/src_select_properties_speech_rate.o $(NS_OBJ)/libcss/src_select_properties_speak.o $(NS_OBJ)/libcss/src_select_properties_speak_header.o $(NS_OBJ)/libcss/src_select_properties_speak_numeral.o $(NS_OBJ)/libcss/src_select_properties_speak_punctuation.o $(NS_OBJ)/libcss/src_select_properties_stress.o $(NS_OBJ)/libcss/src_select_properties_stroke_opacity.o $(NS_OBJ)/libcss/src_select_properties_table_layout.o $(NS_OBJ)/libcss/src_select_properties_text_align.o $(NS_OBJ)/libcss/src_select_properties_text_decoration.o $(NS_OBJ)/libcss/src_select_properties_text_indent.o $(NS_OBJ)/libcss/src_select_properties_text_transform.o $(NS_OBJ)/libcss/src_select_properties_top.o $(NS_OBJ)/libcss/src_select_properties_unicode_bidi.o $(NS_OBJ)/libcss/src_select_properties_vertical_align.o $(NS_OBJ)/libcss/src_select_properties_visibility.o $(NS_OBJ)/libcss/src_select_properties_voice_family.o $(NS_OBJ)/libcss/src_select_properties_volume.o $(NS_OBJ)/libcss/src_select_properties_white_space.o $(NS_OBJ)/libcss/src_select_properties_widows.o $(NS_OBJ)/libcss/src_select_properties_width.o $(NS_OBJ)/libcss/src_select_properties_word_spacing.o $(NS_OBJ)/libcss/src_select_properties_writing_mode.o $(NS_OBJ)/libcss/src_select_properties_z_index.o $(NS_OBJ)/libcss/src_utils_errors.o $(NS_OBJ)/libcss/src_utils_utils.o
NS_OBJS_libnsutils := $(NS_OBJ)/libnsutils/src_base64.o $(NS_OBJ)/libnsutils/src_time.o $(NS_OBJ)/libnsutils/src_unistd.o
NS_OBJS_libnsgif := $(NS_OBJ)/libnsgif/src_gif.o $(NS_OBJ)/libnsgif/src_lzw.o
NS_OBJS_libnsbmp := $(NS_OBJ)/libnsbmp/src_libnsbmp.o
NS_OBJS_libnspsl := $(NS_OBJ)/libnspsl/src_nspsl.o
NS_OBJS_libnslog := $(NS_OBJ)/libnslog/build-release-x86_64-myos-release-lib-static_filter-parser.o $(NS_OBJ)/libnslog/build-release-x86_64-myos-release-lib-static_filter-lexer.o $(NS_OBJ)/libnslog/src_core.o $(NS_OBJ)/libnslog/src_filter.o
NS_OBJS_libnsfb := $(NS_OBJ)/libnsfb/src_libnsfb.o $(NS_OBJ)/libnsfb/src_dump.o $(NS_OBJ)/libnsfb/src_cursor.o $(NS_OBJ)/libnsfb/src_palette.o $(NS_OBJ)/libnsfb/src_plot_api.o $(NS_OBJ)/libnsfb/src_plot_util.o $(NS_OBJ)/libnsfb/src_plot_generic.o $(NS_OBJ)/libnsfb/src_plot_32bpp-xrgb8888.o $(NS_OBJ)/libnsfb/src_plot_32bpp-xbgr8888.o $(NS_OBJ)/libnsfb/src_plot_16bpp.o $(NS_OBJ)/libnsfb/src_plot_8bpp.o $(NS_OBJ)/libnsfb/src_surface_surface.o $(NS_OBJ)/libnsfb/src_surface_ram.o $(NS_OBJ)/libnsfb/src_surface_myos.o
NS_OBJS_netsurf := $(NS_OBJ)/netsurf/content_content.o $(NS_OBJ)/netsurf/content_content_factory.o $(NS_OBJ)/netsurf/content_fetch.o $(NS_OBJ)/netsurf/content_fetchers_about_about.o $(NS_OBJ)/netsurf/content_fetchers_about_blank.o $(NS_OBJ)/netsurf/content_fetchers_about_certificate.o $(NS_OBJ)/netsurf/content_fetchers_about_chart.o $(NS_OBJ)/netsurf/content_fetchers_about_choices.o $(NS_OBJ)/netsurf/content_fetchers_about_config.o $(NS_OBJ)/netsurf/content_fetchers_about_imagecache.o $(NS_OBJ)/netsurf/content_fetchers_about_nscolours.o $(NS_OBJ)/netsurf/content_fetchers_about_query_auth.o $(NS_OBJ)/netsurf/content_fetchers_about_query.o $(NS_OBJ)/netsurf/content_fetchers_about_query_fetcherror.o $(NS_OBJ)/netsurf/content_fetchers_about_query_privacy.o $(NS_OBJ)/netsurf/content_fetchers_about_query_timeout.o $(NS_OBJ)/netsurf/content_fetchers_about_testament.o $(NS_OBJ)/netsurf/content_fetchers_about_websearch.o $(NS_OBJ)/netsurf/content_fetchers_curl.o $(NS_OBJ)/netsurf/content_fetchers_file_dirlist.o $(NS_OBJ)/netsurf/content_fetchers_data.o $(NS_OBJ)/netsurf/content_fetchers_file_file.o $(NS_OBJ)/netsurf/content_fetchers_resource.o $(NS_OBJ)/netsurf/content_handlers_css_css.o $(NS_OBJ)/netsurf/content_handlers_css_dump.o $(NS_OBJ)/netsurf/content_handlers_css_hints.o $(NS_OBJ)/netsurf/content_handlers_css_internal.o $(NS_OBJ)/netsurf/content_handlers_css_select.o $(NS_OBJ)/netsurf/content_handlers_html_box_construct.o $(NS_OBJ)/netsurf/content_handlers_html_box_inspect.o $(NS_OBJ)/netsurf/content_handlers_html_box_manipulate.o $(NS_OBJ)/netsurf/content_handlers_html_box_normalise.o $(NS_OBJ)/netsurf/content_handlers_html_box_textarea.o $(NS_OBJ)/netsurf/content_handlers_html_box_special.o $(NS_OBJ)/netsurf/content_handlers_html_css.o $(NS_OBJ)/netsurf/content_handlers_html_css_fetcher.o $(NS_OBJ)/netsurf/content_handlers_html_dom_event.o $(NS_OBJ)/netsurf/content_handlers_html_font.o $(NS_OBJ)/netsurf/content_handlers_html_form.o $(NS_OBJ)/netsurf/content_handlers_html_forms.o $(NS_OBJ)/netsurf/content_handlers_html_html.o $(NS_OBJ)/netsurf/content_handlers_html_imagemap.o $(NS_OBJ)/netsurf/content_handlers_html_interaction.o $(NS_OBJ)/netsurf/content_handlers_html_layout.o $(NS_OBJ)/netsurf/content_handlers_html_layout_flex.o $(NS_OBJ)/netsurf/content_handlers_html_object.o $(NS_OBJ)/netsurf/content_handlers_html_redraw.o $(NS_OBJ)/netsurf/content_handlers_html_redraw_border.o $(NS_OBJ)/netsurf/content_handlers_html_script.o $(NS_OBJ)/netsurf/content_handlers_html_table.o $(NS_OBJ)/netsurf/content_handlers_html_textselection.o $(NS_OBJ)/netsurf/content_handlers_image_bmp.o $(NS_OBJ)/netsurf/content_handlers_image_gif.o $(NS_OBJ)/netsurf/content_handlers_image_ico.o $(NS_OBJ)/netsurf/content_handlers_image_image.o $(NS_OBJ)/netsurf/content_handlers_image_image_cache.o $(NS_OBJ)/netsurf/content_handlers_image_jpeg.o $(NS_OBJ)/netsurf/content_handlers_image_png.o $(NS_OBJ)/netsurf/content_handlers_javascript_fetcher.o $(NS_OBJ)/netsurf/content_handlers_javascript_none_none.o $(NS_OBJ)/netsurf/content_handlers_text_textplain.o $(NS_OBJ)/netsurf/content_hlcache.o $(NS_OBJ)/netsurf/content_llcache.o $(NS_OBJ)/netsurf/content_mimesniff.o $(NS_OBJ)/netsurf/content_no_backing_store.o $(NS_OBJ)/netsurf/content_textsearch.o $(NS_OBJ)/netsurf/content_urldb.o $(NS_OBJ)/netsurf/desktop_bitmap.o $(NS_OBJ)/netsurf/desktop_browser.o $(NS_OBJ)/netsurf/desktop_browser_history.o $(NS_OBJ)/netsurf/desktop_browser_window.o $(NS_OBJ)/netsurf/desktop_cookie_manager.o $(NS_OBJ)/netsurf/desktop_cw_helper.o $(NS_OBJ)/netsurf/desktop_download.o $(NS_OBJ)/netsurf/desktop_font_haru.o $(NS_OBJ)/netsurf/desktop_frames.o $(NS_OBJ)/netsurf/desktop_global_history.o $(NS_OBJ)/netsurf/desktop_hotlist.o $(NS_OBJ)/netsurf/desktop_gui_factory.o $(NS_OBJ)/netsurf/desktop_knockout.o $(NS_OBJ)/netsurf/desktop_local_history.o $(NS_OBJ)/netsurf/desktop_mouse.o $(NS_OBJ)/netsurf/desktop_netsurf.o $(NS_OBJ)/netsurf/desktop_page-info.o $(NS_OBJ)/netsurf/desktop_plot_style.o $(NS_OBJ)/netsurf/desktop_print.o $(NS_OBJ)/netsurf/desktop_save_complete.o $(NS_OBJ)/netsurf/desktop_save_pdf.o $(NS_OBJ)/netsurf/desktop_scrollbar.o $(NS_OBJ)/netsurf/desktop_save_text.o $(NS_OBJ)/netsurf/desktop_search.o $(NS_OBJ)/netsurf/desktop_searchweb.o $(NS_OBJ)/netsurf/desktop_selection.o $(NS_OBJ)/netsurf/desktop_system_colour.o $(NS_OBJ)/netsurf/desktop_textarea.o $(NS_OBJ)/netsurf/desktop_textinput.o $(NS_OBJ)/netsurf/desktop_treeview.o $(NS_OBJ)/netsurf/desktop_version.o $(NS_OBJ)/netsurf/frontends_framebuffer_bitmap.o $(NS_OBJ)/netsurf/frontends_framebuffer_clipboard.o $(NS_OBJ)/netsurf/frontends_framebuffer_corewindow.o $(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_bitmap.o $(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_event.o $(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_fbtk.o $(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_fill.o $(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_osk.o $(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_scroll.o $(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_text.o $(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_user.o $(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_window.o $(NS_OBJ)/netsurf/frontends_framebuffer_fetch.o $(NS_OBJ)/netsurf/frontends_framebuffer_findfile.o $(NS_OBJ)/netsurf/frontends_framebuffer_font_freetype.o $(NS_OBJ)/netsurf/frontends_framebuffer_framebuffer.o $(NS_OBJ)/netsurf/frontends_framebuffer_gui.o $(NS_OBJ)/netsurf/frontends_framebuffer_local_history.o $(NS_OBJ)/netsurf/frontends_framebuffer_schedule.o $(NS_OBJ)/netsurf/utils_bloom.o $(NS_OBJ)/netsurf/utils_corestrings.o $(NS_OBJ)/netsurf/utils_file.o $(NS_OBJ)/netsurf/utils_filepath.o $(NS_OBJ)/netsurf/utils_hashmap.o $(NS_OBJ)/netsurf/utils_hashtable.o $(NS_OBJ)/netsurf/utils_http_cache-control.o $(NS_OBJ)/netsurf/utils_http_challenge.o $(NS_OBJ)/netsurf/utils_http_content-disposition.o $(NS_OBJ)/netsurf/utils_http_content-type.o $(NS_OBJ)/netsurf/utils_http_generics.o $(NS_OBJ)/netsurf/utils_http_parameter.o $(NS_OBJ)/netsurf/utils_http_primitives.o $(NS_OBJ)/netsurf/utils_http_strict-transport-security.o $(NS_OBJ)/netsurf/utils_http_www-authenticate.o $(NS_OBJ)/netsurf/utils_idna.o $(NS_OBJ)/netsurf/utils_libdom.o $(NS_OBJ)/netsurf/utils_log.o $(NS_OBJ)/netsurf/utils_messages.o $(NS_OBJ)/netsurf/utils_nscolour.o $(NS_OBJ)/netsurf/utils_nsurl_nsurl.o $(NS_OBJ)/netsurf/utils_nsoption.o $(NS_OBJ)/netsurf/utils_nsurl_parse.o $(NS_OBJ)/netsurf/utils_punycode.o $(NS_OBJ)/netsurf/utils_ssl_certs.o $(NS_OBJ)/netsurf/utils_talloc.o $(NS_OBJ)/netsurf/utils_time.o $(NS_OBJ)/netsurf/utils_url.o $(NS_OBJ)/netsurf/utils_useragent.o $(NS_OBJ)/netsurf/utils_utf8.o $(NS_OBJ)/netsurf/utils_utils.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-caret_image.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-hand_image.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-history_image.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-history_image_g.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-left_arrow.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-left_arrow_g.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-move_image.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-menu_image.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-osk_image.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-pointer_image.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-progress_image.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-reload.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-right_arrow_g.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-reload_g.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-right_arrow.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-scrolld.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-scrolll.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-scrollr.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-scrollu.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-stop_image.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-stop_image_g.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber0.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber1.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber2.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber3.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber4.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber6.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber5.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber7.o $(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber8.o
NS_OBJS_libjpeg := $(NS_OBJ)/libjpeg/jcapimin.c.o $(NS_OBJ)/libjpeg/jcapistd-8.c.o $(NS_OBJ)/libjpeg/jcapistd-12.c.o $(NS_OBJ)/libjpeg/jcapistd-16.c.o $(NS_OBJ)/libjpeg/jccoefct-8.c.o $(NS_OBJ)/libjpeg/jccoefct-12.c.o $(NS_OBJ)/libjpeg/jccolor-8.c.o $(NS_OBJ)/libjpeg/jccolor-12.c.o $(NS_OBJ)/libjpeg/jccolor-16.c.o $(NS_OBJ)/libjpeg/jcdctmgr-8.c.o $(NS_OBJ)/libjpeg/jcdctmgr-12.c.o $(NS_OBJ)/libjpeg/jcdiffct-8.c.o $(NS_OBJ)/libjpeg/jcdiffct-12.c.o $(NS_OBJ)/libjpeg/jcdiffct-16.c.o $(NS_OBJ)/libjpeg/jchuff.c.o $(NS_OBJ)/libjpeg/jcicc.c.o $(NS_OBJ)/libjpeg/jcinit.c.o $(NS_OBJ)/libjpeg/jclhuff.c.o $(NS_OBJ)/libjpeg/jclossls-8.c.o $(NS_OBJ)/libjpeg/jclossls-12.c.o $(NS_OBJ)/libjpeg/jclossls-16.c.o $(NS_OBJ)/libjpeg/jcmainct-8.c.o $(NS_OBJ)/libjpeg/jcmainct-12.c.o $(NS_OBJ)/libjpeg/jcmainct-16.c.o $(NS_OBJ)/libjpeg/jcmarker.c.o $(NS_OBJ)/libjpeg/jcmaster.c.o $(NS_OBJ)/libjpeg/jcomapi.c.o $(NS_OBJ)/libjpeg/jcparam.c.o $(NS_OBJ)/libjpeg/jcphuff.c.o $(NS_OBJ)/libjpeg/jcprepct-8.c.o $(NS_OBJ)/libjpeg/jcprepct-12.c.o $(NS_OBJ)/libjpeg/jcprepct-16.c.o $(NS_OBJ)/libjpeg/jcsample-8.c.o $(NS_OBJ)/libjpeg/jcsample-12.c.o $(NS_OBJ)/libjpeg/jcsample-16.c.o $(NS_OBJ)/libjpeg/jctrans.c.o $(NS_OBJ)/libjpeg/jdapimin.c.o $(NS_OBJ)/libjpeg/jdapistd-8.c.o $(NS_OBJ)/libjpeg/jdapistd-12.c.o $(NS_OBJ)/libjpeg/jdapistd-16.c.o $(NS_OBJ)/libjpeg/jdatadst.c.o $(NS_OBJ)/libjpeg/jdatasrc.c.o $(NS_OBJ)/libjpeg/jdcoefct-8.c.o $(NS_OBJ)/libjpeg/jdcoefct-12.c.o $(NS_OBJ)/libjpeg/jdcolor-8.c.o $(NS_OBJ)/libjpeg/jdcolor-12.c.o $(NS_OBJ)/libjpeg/jdcolor-16.c.o $(NS_OBJ)/libjpeg/jddctmgr-8.c.o $(NS_OBJ)/libjpeg/jddctmgr-12.c.o $(NS_OBJ)/libjpeg/jddiffct-8.c.o $(NS_OBJ)/libjpeg/jddiffct-12.c.o $(NS_OBJ)/libjpeg/jddiffct-16.c.o $(NS_OBJ)/libjpeg/jdhuff.c.o $(NS_OBJ)/libjpeg/jdicc.c.o $(NS_OBJ)/libjpeg/jdinput.c.o $(NS_OBJ)/libjpeg/jdlhuff.c.o $(NS_OBJ)/libjpeg/jdlossls-8.c.o $(NS_OBJ)/libjpeg/jdlossls-12.c.o $(NS_OBJ)/libjpeg/jdlossls-16.c.o $(NS_OBJ)/libjpeg/jdmainct-8.c.o $(NS_OBJ)/libjpeg/jdmainct-12.c.o $(NS_OBJ)/libjpeg/jdmainct-16.c.o $(NS_OBJ)/libjpeg/jdmarker.c.o $(NS_OBJ)/libjpeg/jdmaster.c.o $(NS_OBJ)/libjpeg/jdmerge-8.c.o $(NS_OBJ)/libjpeg/jdmerge-12.c.o $(NS_OBJ)/libjpeg/jdphuff.c.o $(NS_OBJ)/libjpeg/jdpostct-8.c.o $(NS_OBJ)/libjpeg/jdpostct-12.c.o $(NS_OBJ)/libjpeg/jdpostct-16.c.o $(NS_OBJ)/libjpeg/jdsample-8.c.o $(NS_OBJ)/libjpeg/jdsample-12.c.o $(NS_OBJ)/libjpeg/jdsample-16.c.o $(NS_OBJ)/libjpeg/jdtrans.c.o $(NS_OBJ)/libjpeg/jerror.c.o $(NS_OBJ)/libjpeg/jfdctflt.c.o $(NS_OBJ)/libjpeg/jfdctfst-8.c.o $(NS_OBJ)/libjpeg/jfdctfst-12.c.o $(NS_OBJ)/libjpeg/jfdctint-8.c.o $(NS_OBJ)/libjpeg/jfdctint-12.c.o $(NS_OBJ)/libjpeg/jidctflt-8.c.o $(NS_OBJ)/libjpeg/jidctflt-12.c.o $(NS_OBJ)/libjpeg/jidctfst-8.c.o $(NS_OBJ)/libjpeg/jidctfst-12.c.o $(NS_OBJ)/libjpeg/jidctint-8.c.o $(NS_OBJ)/libjpeg/jidctint-12.c.o $(NS_OBJ)/libjpeg/jidctred-8.c.o $(NS_OBJ)/libjpeg/jidctred-12.c.o $(NS_OBJ)/libjpeg/jmemmgr.c.o $(NS_OBJ)/libjpeg/jmemnobs.c.o $(NS_OBJ)/libjpeg/jpeg_nbits.c.o $(NS_OBJ)/libjpeg/jquant1-8.c.o $(NS_OBJ)/libjpeg/jquant1-12.c.o $(NS_OBJ)/libjpeg/jquant2-8.c.o $(NS_OBJ)/libjpeg/jquant2-12.c.o $(NS_OBJ)/libjpeg/jutils-8.c.o $(NS_OBJ)/libjpeg/jutils-12.c.o $(NS_OBJ)/libjpeg/jutils-16.c.o $(NS_OBJ)/libjpeg/jaricom.c.o $(NS_OBJ)/libjpeg/jcarith.c.o $(NS_OBJ)/libjpeg/jdarith.c.o
NS_OBJS_freetype := $(NS_OBJ)/freetype/autofit.c.o $(NS_OBJ)/freetype/ftbase.c.o $(NS_OBJ)/freetype/ftbbox.c.o $(NS_OBJ)/freetype/ftbdf.c.o $(NS_OBJ)/freetype/ftbitmap.c.o $(NS_OBJ)/freetype/ftcid.c.o $(NS_OBJ)/freetype/ftfstype.c.o $(NS_OBJ)/freetype/ftgasp.c.o $(NS_OBJ)/freetype/ftglyph.c.o $(NS_OBJ)/freetype/ftgxval.c.o $(NS_OBJ)/freetype/ftinit.c.o $(NS_OBJ)/freetype/ftmm.c.o $(NS_OBJ)/freetype/ftotval.c.o $(NS_OBJ)/freetype/ftpatent.c.o $(NS_OBJ)/freetype/ftpfr.c.o $(NS_OBJ)/freetype/ftstroke.c.o $(NS_OBJ)/freetype/ftsynth.c.o $(NS_OBJ)/freetype/fttype1.c.o $(NS_OBJ)/freetype/ftwinfnt.c.o $(NS_OBJ)/freetype/bdf.c.o $(NS_OBJ)/freetype/ftbzip2.c.o $(NS_OBJ)/freetype/ftcache.c.o $(NS_OBJ)/freetype/cff.c.o $(NS_OBJ)/freetype/type1cid.c.o $(NS_OBJ)/freetype/ftgzip.c.o $(NS_OBJ)/freetype/ftlzw.c.o $(NS_OBJ)/freetype/pcf.c.o $(NS_OBJ)/freetype/pfr.c.o $(NS_OBJ)/freetype/psaux.c.o $(NS_OBJ)/freetype/pshinter.c.o $(NS_OBJ)/freetype/psnames.c.o $(NS_OBJ)/freetype/raster.c.o $(NS_OBJ)/freetype/sdf.c.o $(NS_OBJ)/freetype/sfnt.c.o $(NS_OBJ)/freetype/smooth.c.o $(NS_OBJ)/freetype/svg.c.o $(NS_OBJ)/freetype/truetype.c.o $(NS_OBJ)/freetype/type1.c.o $(NS_OBJ)/freetype/type42.c.o $(NS_OBJ)/freetype/winfnt.c.o $(NS_OBJ)/freetype/ftsystem.c.o $(NS_OBJ)/freetype/ftdebug.c.o
NS_OBJS_utf8proc := $(NS_OBJ)/utf8proc/utf8proc.o
NS_OBJS_libpng := $(NS_OBJ)/libpng/png.o $(NS_OBJ)/libpng/pngerror.o $(NS_OBJ)/libpng/pngget.o $(NS_OBJ)/libpng/pngmem.o $(NS_OBJ)/libpng/pngpread.o $(NS_OBJ)/libpng/pngread.o $(NS_OBJ)/libpng/pngrio.o $(NS_OBJ)/libpng/pngrtran.o $(NS_OBJ)/libpng/pngrutil.o $(NS_OBJ)/libpng/pngset.o $(NS_OBJ)/libpng/pngtrans.o $(NS_OBJ)/libpng/pngwio.o $(NS_OBJ)/libpng/pngwrite.o $(NS_OBJ)/libpng/pngwtran.o $(NS_OBJ)/libpng/pngwutil.o
NS_COMPONENTS := libwapcaplet libparserutils libhubbub libdom libcss libnsutils libnsgif libnsbmp libnspsl libnslog libnsfb netsurf libjpeg freetype utf8proc libpng

$(NS_OBJ)/libwapcaplet/src_libwapcaplet.o: $(TP)/netsurf/libwapcaplet/src/libwapcaplet.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libwapcaplet] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libwapcaplet/include' '-I$(TP)/netsurf/libwapcaplet/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_charset_aliases.o: $(TP)/netsurf/libparserutils/src/charset/aliases.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_charset_codec.o: $(TP)/netsurf/libparserutils/src/charset/codec.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_charset_codecs_codec_ascii.o: $(TP)/netsurf/libparserutils/src/charset/codecs/codec_ascii.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_charset_codecs_codec_8859.o: $(TP)/netsurf/libparserutils/src/charset/codecs/codec_8859.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_charset_codecs_codec_ext8.o: $(TP)/netsurf/libparserutils/src/charset/codecs/codec_ext8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_charset_codecs_codec_utf8.o: $(TP)/netsurf/libparserutils/src/charset/codecs/codec_utf8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_charset_codecs_codec_utf16.o: $(TP)/netsurf/libparserutils/src/charset/codecs/codec_utf16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_charset_encodings_utf8.o: $(TP)/netsurf/libparserutils/src/charset/encodings/utf8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_charset_encodings_utf16.o: $(TP)/netsurf/libparserutils/src/charset/encodings/utf16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_input_filter.o: $(TP)/netsurf/libparserutils/src/input/filter.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_input_inputstream.o: $(TP)/netsurf/libparserutils/src/input/inputstream.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_utils_buffer.o: $(TP)/netsurf/libparserutils/src/utils/buffer.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_utils_errors.o: $(TP)/netsurf/libparserutils/src/utils/errors.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_utils_stack.o: $(TP)/netsurf/libparserutils/src/utils/stack.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libparserutils/src_utils_vector.o: $(TP)/netsurf/libparserutils/src/utils/vector.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libparserutils] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libparserutils/include' '-I$(TP)/netsurf/libparserutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -DWITHOUT_ICONV_FILTER '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 '-I$(TP)/netsurf/libparserutils/test' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_parser.o: $(TP)/netsurf/libhubbub/src/parser.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_charset_detect.o: $(TP)/netsurf/libhubbub/src/charset/detect.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_tokeniser_entities.o: $(TP)/netsurf/libhubbub/src/tokeniser/entities.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_tokeniser_tokeniser.o: $(TP)/netsurf/libhubbub/src/tokeniser/tokeniser.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_treebuilder.o: $(TP)/netsurf/libhubbub/src/treebuilder/treebuilder.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_initial.o: $(TP)/netsurf/libhubbub/src/treebuilder/initial.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_before_html.o: $(TP)/netsurf/libhubbub/src/treebuilder/before_html.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_before_head.o: $(TP)/netsurf/libhubbub/src/treebuilder/before_head.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_head.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_head.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_head_noscript.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_head_noscript.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_after_head.o: $(TP)/netsurf/libhubbub/src/treebuilder/after_head.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_body.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_body.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_table.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_table.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_caption.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_caption.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_column_group.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_column_group.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_table_body.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_table_body.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_row.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_row.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_cell.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_cell.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_select.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_select.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_select_in_table.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_select_in_table.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_foreign_content.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_foreign_content.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_after_body.o: $(TP)/netsurf/libhubbub/src/treebuilder/after_body.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_in_frameset.o: $(TP)/netsurf/libhubbub/src/treebuilder/in_frameset.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_after_frameset.o: $(TP)/netsurf/libhubbub/src/treebuilder/after_frameset.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_after_after_body.o: $(TP)/netsurf/libhubbub/src/treebuilder/after_after_body.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_after_after_frameset.o: $(TP)/netsurf/libhubbub/src/treebuilder/after_after_frameset.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_generic_rcdata.o: $(TP)/netsurf/libhubbub/src/treebuilder/generic_rcdata.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_treebuilder_element-type.o: $(TP)/netsurf/libhubbub/src/treebuilder/element-type.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_utils_errors.o: $(TP)/netsurf/libhubbub/src/utils/errors.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libhubbub/src_utils_string.o: $(TP)/netsurf/libhubbub/src/utils/string.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libhubbub] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libhubbub/include' '-I$(TP)/netsurf/libhubbub/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/bindings_hubbub_parser.o: $(TP)/netsurf/libdom/bindings/hubbub/parser.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_string.o: $(TP)/netsurf/libdom/src/core/string.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_node.o: $(TP)/netsurf/libdom/src/core/node.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_attr.o: $(TP)/netsurf/libdom/src/core/attr.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_characterdata.o: $(TP)/netsurf/libdom/src/core/characterdata.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_element.o: $(TP)/netsurf/libdom/src/core/element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_implementation.o: $(TP)/netsurf/libdom/src/core/implementation.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_text.o: $(TP)/netsurf/libdom/src/core/text.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_typeinfo.o: $(TP)/netsurf/libdom/src/core/typeinfo.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_comment.o: $(TP)/netsurf/libdom/src/core/comment.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_namednodemap.o: $(TP)/netsurf/libdom/src/core/namednodemap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_nodelist.o: $(TP)/netsurf/libdom/src/core/nodelist.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_cdatasection.o: $(TP)/netsurf/libdom/src/core/cdatasection.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_document_type.o: $(TP)/netsurf/libdom/src/core/document_type.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_entity_ref.o: $(TP)/netsurf/libdom/src/core/entity_ref.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_pi.o: $(TP)/netsurf/libdom/src/core/pi.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_doc_fragment.o: $(TP)/netsurf/libdom/src/core/doc_fragment.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_document.o: $(TP)/netsurf/libdom/src/core/document.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_core_tokenlist.o: $(TP)/netsurf/libdom/src/core/tokenlist.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_event.o: $(TP)/netsurf/libdom/src/events/event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_dispatch.o: $(TP)/netsurf/libdom/src/events/dispatch.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_event_target.o: $(TP)/netsurf/libdom/src/events/event_target.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_document_event.o: $(TP)/netsurf/libdom/src/events/document_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_custom_event.o: $(TP)/netsurf/libdom/src/events/custom_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_keyboard_event.o: $(TP)/netsurf/libdom/src/events/keyboard_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_mouse_wheel_event.o: $(TP)/netsurf/libdom/src/events/mouse_wheel_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_text_event.o: $(TP)/netsurf/libdom/src/events/text_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_event_listener.o: $(TP)/netsurf/libdom/src/events/event_listener.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_mouse_event.o: $(TP)/netsurf/libdom/src/events/mouse_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_mutation_event.o: $(TP)/netsurf/libdom/src/events/mutation_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_ui_event.o: $(TP)/netsurf/libdom/src/events/ui_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_mouse_multi_wheel_event.o: $(TP)/netsurf/libdom/src/events/mouse_multi_wheel_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_events_mutation_name_event.o: $(TP)/netsurf/libdom/src/events/mutation_name_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_applet_element.o: $(TP)/netsurf/libdom/src/html/html_applet_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_area_element.o: $(TP)/netsurf/libdom/src/html/html_area_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_anchor_element.o: $(TP)/netsurf/libdom/src/html/html_anchor_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_basefont_element.o: $(TP)/netsurf/libdom/src/html/html_basefont_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_base_element.o: $(TP)/netsurf/libdom/src/html/html_base_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_body_element.o: $(TP)/netsurf/libdom/src/html/html_body_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_button_element.o: $(TP)/netsurf/libdom/src/html/html_button_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_canvas_element.o: $(TP)/netsurf/libdom/src/html/html_canvas_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_collection.o: $(TP)/netsurf/libdom/src/html/html_collection.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_document.o: $(TP)/netsurf/libdom/src/html/html_document.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_element.o: $(TP)/netsurf/libdom/src/html/html_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_dlist_element.o: $(TP)/netsurf/libdom/src/html/html_dlist_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_directory_element.o: $(TP)/netsurf/libdom/src/html/html_directory_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_options_collection.o: $(TP)/netsurf/libdom/src/html/html_options_collection.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_html_element.o: $(TP)/netsurf/libdom/src/html/html_html_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_head_element.o: $(TP)/netsurf/libdom/src/html/html_head_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_link_element.o: $(TP)/netsurf/libdom/src/html/html_link_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_title_element.o: $(TP)/netsurf/libdom/src/html/html_title_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_meta_element.o: $(TP)/netsurf/libdom/src/html/html_meta_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_style_element.o: $(TP)/netsurf/libdom/src/html/html_style_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_form_element.o: $(TP)/netsurf/libdom/src/html/html_form_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_select_element.o: $(TP)/netsurf/libdom/src/html/html_select_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_input_element.o: $(TP)/netsurf/libdom/src/html/html_input_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_text_area_element.o: $(TP)/netsurf/libdom/src/html/html_text_area_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_opt_group_element.o: $(TP)/netsurf/libdom/src/html/html_opt_group_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_option_element.o: $(TP)/netsurf/libdom/src/html/html_option_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_hr_element.o: $(TP)/netsurf/libdom/src/html/html_hr_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_menu_element.o: $(TP)/netsurf/libdom/src/html/html_menu_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_fieldset_element.o: $(TP)/netsurf/libdom/src/html/html_fieldset_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_legend_element.o: $(TP)/netsurf/libdom/src/html/html_legend_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_div_element.o: $(TP)/netsurf/libdom/src/html/html_div_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_paragraph_element.o: $(TP)/netsurf/libdom/src/html/html_paragraph_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_heading_element.o: $(TP)/netsurf/libdom/src/html/html_heading_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_quote_element.o: $(TP)/netsurf/libdom/src/html/html_quote_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_pre_element.o: $(TP)/netsurf/libdom/src/html/html_pre_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_br_element.o: $(TP)/netsurf/libdom/src/html/html_br_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_label_element.o: $(TP)/netsurf/libdom/src/html/html_label_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_ulist_element.o: $(TP)/netsurf/libdom/src/html/html_ulist_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_olist_element.o: $(TP)/netsurf/libdom/src/html/html_olist_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_li_element.o: $(TP)/netsurf/libdom/src/html/html_li_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_font_element.o: $(TP)/netsurf/libdom/src/html/html_font_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_mod_element.o: $(TP)/netsurf/libdom/src/html/html_mod_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_image_element.o: $(TP)/netsurf/libdom/src/html/html_image_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_object_element.o: $(TP)/netsurf/libdom/src/html/html_object_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_param_element.o: $(TP)/netsurf/libdom/src/html/html_param_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_map_element.o: $(TP)/netsurf/libdom/src/html/html_map_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_script_element.o: $(TP)/netsurf/libdom/src/html/html_script_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_tablecaption_element.o: $(TP)/netsurf/libdom/src/html/html_tablecaption_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_tablecell_element.o: $(TP)/netsurf/libdom/src/html/html_tablecell_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_tablecol_element.o: $(TP)/netsurf/libdom/src/html/html_tablecol_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_tablesection_element.o: $(TP)/netsurf/libdom/src/html/html_tablesection_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_table_element.o: $(TP)/netsurf/libdom/src/html/html_table_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_tablerow_element.o: $(TP)/netsurf/libdom/src/html/html_tablerow_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_frameset_element.o: $(TP)/netsurf/libdom/src/html/html_frameset_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_frame_element.o: $(TP)/netsurf/libdom/src/html/html_frame_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_iframe_element.o: $(TP)/netsurf/libdom/src/html/html_iframe_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_html_html_isindex_element.o: $(TP)/netsurf/libdom/src/html/html_isindex_element.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_utils_namespace.o: $(TP)/netsurf/libdom/src/utils/namespace.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_utils_hashtable.o: $(TP)/netsurf/libdom/src/utils/hashtable.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_utils_character_valid.o: $(TP)/netsurf/libdom/src/utils/character_valid.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_utils_validate.o: $(TP)/netsurf/libdom/src/utils/validate.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libdom/src_utils_walk.o: $(TP)/netsurf/libdom/src/utils/walk.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libdom] $<"
	@$(CC) $(NS_CFLAGS) -std=c99 -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libdom/include' '-I$(TP)/netsurf/libdom/src' '-I$(TP)/netsurf/libdom/binding' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_stylesheet.o: $(TP)/netsurf/libcss/src/stylesheet.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_charset_detect.o: $(TP)/netsurf/libcss/src/charset/detect.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_lex_lex.o: $(TP)/netsurf/libcss/src/lex/lex.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_parse.o: $(TP)/netsurf/libcss/src/parse/parse.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_language.o: $(TP)/netsurf/libcss/src/parse/language.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_important.o: $(TP)/netsurf/libcss/src/parse/important.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_propstrings.o: $(TP)/netsurf/libcss/src/parse/propstrings.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_font_face.o: $(TP)/netsurf/libcss/src/parse/font_face.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_mq.o: $(TP)/netsurf/libcss/src/parse/mq.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_azimuth.o: $(TP)/netsurf/libcss/src/parse/properties/azimuth.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_background.o: $(TP)/netsurf/libcss/src/parse/properties/background.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_background_position.o: $(TP)/netsurf/libcss/src/parse/properties/background_position.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_border.o: $(TP)/netsurf/libcss/src/parse/properties/border.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_border_color.o: $(TP)/netsurf/libcss/src/parse/properties/border_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_border_spacing.o: $(TP)/netsurf/libcss/src/parse/properties/border_spacing.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_border_style.o: $(TP)/netsurf/libcss/src/parse/properties/border_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_border_width.o: $(TP)/netsurf/libcss/src/parse/properties/border_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_clip.o: $(TP)/netsurf/libcss/src/parse/properties/clip.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_columns.o: $(TP)/netsurf/libcss/src/parse/properties/columns.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_column_rule.o: $(TP)/netsurf/libcss/src/parse/properties/column_rule.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_content.o: $(TP)/netsurf/libcss/src/parse/properties/content.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_cue.o: $(TP)/netsurf/libcss/src/parse/properties/cue.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_cursor.o: $(TP)/netsurf/libcss/src/parse/properties/cursor.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_elevation.o: $(TP)/netsurf/libcss/src/parse/properties/elevation.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_fill_opacity.o: $(TP)/netsurf/libcss/src/parse/properties/fill_opacity.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_flex.o: $(TP)/netsurf/libcss/src/parse/properties/flex.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_flex_flow.o: $(TP)/netsurf/libcss/src/parse/properties/flex_flow.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_font.o: $(TP)/netsurf/libcss/src/parse/properties/font.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_font_family.o: $(TP)/netsurf/libcss/src/parse/properties/font_family.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_font_weight.o: $(TP)/netsurf/libcss/src/parse/properties/font_weight.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_list_style.o: $(TP)/netsurf/libcss/src/parse/properties/list_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_list_style_type.o: $(TP)/netsurf/libcss/src/parse/properties/list_style_type.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_margin.o: $(TP)/netsurf/libcss/src/parse/properties/margin.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_opacity.o: $(TP)/netsurf/libcss/src/parse/properties/opacity.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_outline.o: $(TP)/netsurf/libcss/src/parse/properties/outline.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_overflow.o: $(TP)/netsurf/libcss/src/parse/properties/overflow.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_padding.o: $(TP)/netsurf/libcss/src/parse/properties/padding.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_pause.o: $(TP)/netsurf/libcss/src/parse/properties/pause.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_play_during.o: $(TP)/netsurf/libcss/src/parse/properties/play_during.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_properties.o: $(TP)/netsurf/libcss/src/parse/properties/properties.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_quotes.o: $(TP)/netsurf/libcss/src/parse/properties/quotes.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_stroke_opacity.o: $(TP)/netsurf/libcss/src/parse/properties/stroke_opacity.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_text_decoration.o: $(TP)/netsurf/libcss/src/parse/properties/text_decoration.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_utils.o: $(TP)/netsurf/libcss/src/parse/properties/utils.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_voice_family.o: $(TP)/netsurf/libcss/src/parse/properties/voice_family.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_background_repeat.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_background_repeat.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_collapse.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_collapse.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_cue_after.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_cue_after.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_cue_before.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_cue_before.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_direction.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_direction.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_display.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_display.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_empty_cells.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_empty_cells.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_float.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_float.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_font_size.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_font_size.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_font_style.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_font_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_font_variant.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_font_variant.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_height.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_height.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_letter_spacing.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_letter_spacing.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_line_height.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_line_height.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_top.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_top.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_bottom.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_bottom.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_left.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_left.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_right.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_right.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_max_height.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_max_height.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_max_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_max_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_min_height.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_min_height.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_min_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_min_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_color.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_padding_side.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_padding_side.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_padding_bottom.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_padding_bottom.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_padding_left.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_padding_left.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_padding_top.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_padding_top.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_padding_right.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_padding_right.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_margin_side.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_margin_side.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_margin_top.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_margin_top.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_margin_bottom.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_margin_bottom.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_margin_left.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_margin_left.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_margin_right.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_margin_right.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_side.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_side.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_top.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_top.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_bottom.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_bottom.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_left.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_left.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_right.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_right.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_side_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_side_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_top_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_top_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_bottom_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_bottom_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_left_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_left_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_right_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_right_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_side_style.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_side_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_top_style.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_top_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_bottom_style.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_bottom_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_left_style.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_left_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_right_style.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_right_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_side_color.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_side_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_top_color.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_top_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_bottom_color.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_bottom_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_left_color.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_left_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_border_right_color.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_border_right_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_counter_increment.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_counter_increment.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_counter_reset.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_counter_reset.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_background_attachment.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_background_attachment.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_background_color.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_background_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_caption_side.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_caption_side.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_clear.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_clear.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_background_image.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_background_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_list_style_image.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_list_style_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_list_style_position.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_list_style_position.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_orphans.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_orphans.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_outline_color.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_outline_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_outline_style.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_outline_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_outline_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_outline_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_overflow_x.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_overflow_x.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_overflow_y.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_overflow_y.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_page_break_after.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_page_break_after.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_page_break_before.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_page_break_before.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_page_break_inside.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_page_break_inside.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_pause_after.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_pause_after.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_pause_before.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_pause_before.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_pitch.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_pitch.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_pitch_range.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_pitch_range.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_position.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_position.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_richness.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_richness.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_speak.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_speak.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_speak_header.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_speak_header.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_speak_numeral.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_speak_numeral.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_speak_punctuation.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_speak_punctuation.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_speech_rate.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_speech_rate.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_stress.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_stress.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_table_layout.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_table_layout.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_text_align.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_text_align.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_text_indent.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_text_indent.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_text_transform.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_text_transform.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_unicode_bidi.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_unicode_bidi.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_vertical_align.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_vertical_align.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_visibility.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_visibility.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_volume.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_volume.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_white_space.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_white_space.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_widows.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_widows.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_word_spacing.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_word_spacing.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_z_index.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_z_index.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_break_after.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_break_after.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_break_before.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_break_before.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_break_inside.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_break_inside.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_count.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_column_count.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_fill.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_column_fill.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_gap.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_column_gap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_rule_color.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_column_rule_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_rule_style.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_column_rule_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_rule_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_column_rule_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_span.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_column_span.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_column_width.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_column_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_writing_mode.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_writing_mode.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_box_sizing.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_box_sizing.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_align_content.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_align_content.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_align_items.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_align_items.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_align_self.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_align_self.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_flex_basis.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_flex_basis.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_flex_direction.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_flex_direction.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_flex_grow.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_flex_grow.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_flex_shrink.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_flex_shrink.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_flex_wrap.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_flex_wrap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_justify_content.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_justify_content.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_parse_properties_autogenerated_order.o: $(TP)/netsurf/libcss/src/parse/properties/autogenerated_order.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_arena.o: $(TP)/netsurf/libcss/src/select/arena.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_calc.o: $(TP)/netsurf/libcss/src/select/calc.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_computed.o: $(TP)/netsurf/libcss/src/select/computed.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_dispatch.o: $(TP)/netsurf/libcss/src/select/dispatch.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_hash.o: $(TP)/netsurf/libcss/src/select/hash.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_select.o: $(TP)/netsurf/libcss/src/select/select.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_strings.o: $(TP)/netsurf/libcss/src/select/strings.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_font_face.o: $(TP)/netsurf/libcss/src/select/font_face.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_format_list_style.o: $(TP)/netsurf/libcss/src/select/format_list_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_unit.o: $(TP)/netsurf/libcss/src/select/unit.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_helpers.o: $(TP)/netsurf/libcss/src/select/properties/helpers.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_align_content.o: $(TP)/netsurf/libcss/src/select/properties/align_content.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_align_items.o: $(TP)/netsurf/libcss/src/select/properties/align_items.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_align_self.o: $(TP)/netsurf/libcss/src/select/properties/align_self.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_azimuth.o: $(TP)/netsurf/libcss/src/select/properties/azimuth.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_background_attachment.o: $(TP)/netsurf/libcss/src/select/properties/background_attachment.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_background_color.o: $(TP)/netsurf/libcss/src/select/properties/background_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_background_image.o: $(TP)/netsurf/libcss/src/select/properties/background_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_background_position.o: $(TP)/netsurf/libcss/src/select/properties/background_position.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_background_repeat.o: $(TP)/netsurf/libcss/src/select/properties/background_repeat.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_bottom_color.o: $(TP)/netsurf/libcss/src/select/properties/border_bottom_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_bottom_style.o: $(TP)/netsurf/libcss/src/select/properties/border_bottom_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_bottom_width.o: $(TP)/netsurf/libcss/src/select/properties/border_bottom_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_collapse.o: $(TP)/netsurf/libcss/src/select/properties/border_collapse.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_left_color.o: $(TP)/netsurf/libcss/src/select/properties/border_left_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_left_style.o: $(TP)/netsurf/libcss/src/select/properties/border_left_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_left_width.o: $(TP)/netsurf/libcss/src/select/properties/border_left_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_right_color.o: $(TP)/netsurf/libcss/src/select/properties/border_right_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_right_style.o: $(TP)/netsurf/libcss/src/select/properties/border_right_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_right_width.o: $(TP)/netsurf/libcss/src/select/properties/border_right_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_spacing.o: $(TP)/netsurf/libcss/src/select/properties/border_spacing.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_top_color.o: $(TP)/netsurf/libcss/src/select/properties/border_top_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_top_style.o: $(TP)/netsurf/libcss/src/select/properties/border_top_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_border_top_width.o: $(TP)/netsurf/libcss/src/select/properties/border_top_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_bottom.o: $(TP)/netsurf/libcss/src/select/properties/bottom.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_box_sizing.o: $(TP)/netsurf/libcss/src/select/properties/box_sizing.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_break_after.o: $(TP)/netsurf/libcss/src/select/properties/break_after.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_break_before.o: $(TP)/netsurf/libcss/src/select/properties/break_before.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_break_inside.o: $(TP)/netsurf/libcss/src/select/properties/break_inside.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_caption_side.o: $(TP)/netsurf/libcss/src/select/properties/caption_side.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_clear.o: $(TP)/netsurf/libcss/src/select/properties/clear.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_clip.o: $(TP)/netsurf/libcss/src/select/properties/clip.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_color.o: $(TP)/netsurf/libcss/src/select/properties/color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_column_count.o: $(TP)/netsurf/libcss/src/select/properties/column_count.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_column_fill.o: $(TP)/netsurf/libcss/src/select/properties/column_fill.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_column_gap.o: $(TP)/netsurf/libcss/src/select/properties/column_gap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_column_rule_color.o: $(TP)/netsurf/libcss/src/select/properties/column_rule_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_column_rule_style.o: $(TP)/netsurf/libcss/src/select/properties/column_rule_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_column_rule_width.o: $(TP)/netsurf/libcss/src/select/properties/column_rule_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_column_span.o: $(TP)/netsurf/libcss/src/select/properties/column_span.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_column_width.o: $(TP)/netsurf/libcss/src/select/properties/column_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_content.o: $(TP)/netsurf/libcss/src/select/properties/content.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_counter_increment.o: $(TP)/netsurf/libcss/src/select/properties/counter_increment.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_counter_reset.o: $(TP)/netsurf/libcss/src/select/properties/counter_reset.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_cue_after.o: $(TP)/netsurf/libcss/src/select/properties/cue_after.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_cue_before.o: $(TP)/netsurf/libcss/src/select/properties/cue_before.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_cursor.o: $(TP)/netsurf/libcss/src/select/properties/cursor.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_direction.o: $(TP)/netsurf/libcss/src/select/properties/direction.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_display.o: $(TP)/netsurf/libcss/src/select/properties/display.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_elevation.o: $(TP)/netsurf/libcss/src/select/properties/elevation.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_empty_cells.o: $(TP)/netsurf/libcss/src/select/properties/empty_cells.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_fill_opacity.o: $(TP)/netsurf/libcss/src/select/properties/fill_opacity.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_flex_basis.o: $(TP)/netsurf/libcss/src/select/properties/flex_basis.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_flex_direction.o: $(TP)/netsurf/libcss/src/select/properties/flex_direction.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_flex_grow.o: $(TP)/netsurf/libcss/src/select/properties/flex_grow.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_flex_shrink.o: $(TP)/netsurf/libcss/src/select/properties/flex_shrink.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_flex_wrap.o: $(TP)/netsurf/libcss/src/select/properties/flex_wrap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_float.o: $(TP)/netsurf/libcss/src/select/properties/float.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_font_family.o: $(TP)/netsurf/libcss/src/select/properties/font_family.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_font_size.o: $(TP)/netsurf/libcss/src/select/properties/font_size.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_font_style.o: $(TP)/netsurf/libcss/src/select/properties/font_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_font_variant.o: $(TP)/netsurf/libcss/src/select/properties/font_variant.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_font_weight.o: $(TP)/netsurf/libcss/src/select/properties/font_weight.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_height.o: $(TP)/netsurf/libcss/src/select/properties/height.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_justify_content.o: $(TP)/netsurf/libcss/src/select/properties/justify_content.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_left.o: $(TP)/netsurf/libcss/src/select/properties/left.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_letter_spacing.o: $(TP)/netsurf/libcss/src/select/properties/letter_spacing.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_line_height.o: $(TP)/netsurf/libcss/src/select/properties/line_height.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_list_style_image.o: $(TP)/netsurf/libcss/src/select/properties/list_style_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_list_style_position.o: $(TP)/netsurf/libcss/src/select/properties/list_style_position.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_list_style_type.o: $(TP)/netsurf/libcss/src/select/properties/list_style_type.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_margin_bottom.o: $(TP)/netsurf/libcss/src/select/properties/margin_bottom.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_margin_left.o: $(TP)/netsurf/libcss/src/select/properties/margin_left.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_margin_right.o: $(TP)/netsurf/libcss/src/select/properties/margin_right.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_margin_top.o: $(TP)/netsurf/libcss/src/select/properties/margin_top.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_max_height.o: $(TP)/netsurf/libcss/src/select/properties/max_height.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_max_width.o: $(TP)/netsurf/libcss/src/select/properties/max_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_min_height.o: $(TP)/netsurf/libcss/src/select/properties/min_height.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_min_width.o: $(TP)/netsurf/libcss/src/select/properties/min_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_opacity.o: $(TP)/netsurf/libcss/src/select/properties/opacity.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_order.o: $(TP)/netsurf/libcss/src/select/properties/order.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_orphans.o: $(TP)/netsurf/libcss/src/select/properties/orphans.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_outline_color.o: $(TP)/netsurf/libcss/src/select/properties/outline_color.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_outline_style.o: $(TP)/netsurf/libcss/src/select/properties/outline_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_outline_width.o: $(TP)/netsurf/libcss/src/select/properties/outline_width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_overflow_x.o: $(TP)/netsurf/libcss/src/select/properties/overflow_x.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_overflow_y.o: $(TP)/netsurf/libcss/src/select/properties/overflow_y.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_padding_bottom.o: $(TP)/netsurf/libcss/src/select/properties/padding_bottom.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_padding_left.o: $(TP)/netsurf/libcss/src/select/properties/padding_left.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_padding_right.o: $(TP)/netsurf/libcss/src/select/properties/padding_right.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_padding_top.o: $(TP)/netsurf/libcss/src/select/properties/padding_top.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_page_break_after.o: $(TP)/netsurf/libcss/src/select/properties/page_break_after.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_page_break_before.o: $(TP)/netsurf/libcss/src/select/properties/page_break_before.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_page_break_inside.o: $(TP)/netsurf/libcss/src/select/properties/page_break_inside.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_pause_after.o: $(TP)/netsurf/libcss/src/select/properties/pause_after.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_pause_before.o: $(TP)/netsurf/libcss/src/select/properties/pause_before.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_pitch.o: $(TP)/netsurf/libcss/src/select/properties/pitch.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_pitch_range.o: $(TP)/netsurf/libcss/src/select/properties/pitch_range.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_play_during.o: $(TP)/netsurf/libcss/src/select/properties/play_during.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_position.o: $(TP)/netsurf/libcss/src/select/properties/position.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_quotes.o: $(TP)/netsurf/libcss/src/select/properties/quotes.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_richness.o: $(TP)/netsurf/libcss/src/select/properties/richness.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_right.o: $(TP)/netsurf/libcss/src/select/properties/right.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_speech_rate.o: $(TP)/netsurf/libcss/src/select/properties/speech_rate.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_speak.o: $(TP)/netsurf/libcss/src/select/properties/speak.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_speak_header.o: $(TP)/netsurf/libcss/src/select/properties/speak_header.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_speak_numeral.o: $(TP)/netsurf/libcss/src/select/properties/speak_numeral.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_speak_punctuation.o: $(TP)/netsurf/libcss/src/select/properties/speak_punctuation.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_stress.o: $(TP)/netsurf/libcss/src/select/properties/stress.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_stroke_opacity.o: $(TP)/netsurf/libcss/src/select/properties/stroke_opacity.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_table_layout.o: $(TP)/netsurf/libcss/src/select/properties/table_layout.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_text_align.o: $(TP)/netsurf/libcss/src/select/properties/text_align.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_text_decoration.o: $(TP)/netsurf/libcss/src/select/properties/text_decoration.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_text_indent.o: $(TP)/netsurf/libcss/src/select/properties/text_indent.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_text_transform.o: $(TP)/netsurf/libcss/src/select/properties/text_transform.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_top.o: $(TP)/netsurf/libcss/src/select/properties/top.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_unicode_bidi.o: $(TP)/netsurf/libcss/src/select/properties/unicode_bidi.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_vertical_align.o: $(TP)/netsurf/libcss/src/select/properties/vertical_align.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_visibility.o: $(TP)/netsurf/libcss/src/select/properties/visibility.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_voice_family.o: $(TP)/netsurf/libcss/src/select/properties/voice_family.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_volume.o: $(TP)/netsurf/libcss/src/select/properties/volume.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_white_space.o: $(TP)/netsurf/libcss/src/select/properties/white_space.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_widows.o: $(TP)/netsurf/libcss/src/select/properties/widows.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_width.o: $(TP)/netsurf/libcss/src/select/properties/width.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_word_spacing.o: $(TP)/netsurf/libcss/src/select/properties/word_spacing.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_writing_mode.o: $(TP)/netsurf/libcss/src/select/properties/writing_mode.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_select_properties_z_index.o: $(TP)/netsurf/libcss/src/select/properties/z_index.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_utils_errors.o: $(TP)/netsurf/libcss/src/utils/errors.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libcss/src_utils_utils.o: $(TP)/netsurf/libcss/src/utils/utils.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libcss] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libcss/include' '-I$(TP)/netsurf/libcss/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsutils/src_base64.o: $(TP)/netsurf/libnsutils/src/base64.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsutils] $<"
	@$(CC) $(NS_CFLAGS) -D_GNU_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libnsutils/include' '-I$(TP)/netsurf/libnsutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 -D_POSIX_C_SOURCE=200809L $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsutils/src_time.o: $(TP)/netsurf/libnsutils/src/time.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsutils] $<"
	@$(CC) $(NS_CFLAGS) -D_GNU_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libnsutils/include' '-I$(TP)/netsurf/libnsutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 -D_POSIX_C_SOURCE=200809L $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsutils/src_unistd.o: $(TP)/netsurf/libnsutils/src/unistd.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsutils] $<"
	@$(CC) $(NS_CFLAGS) -D_GNU_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libnsutils/include' '-I$(TP)/netsurf/libnsutils/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 -D_POSIX_C_SOURCE=200809L $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsgif/src_gif.o: $(TP)/netsurf/libnsgif/src/gif.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsgif] $<"
	@$(CC) $(NS_CFLAGS) -DNSGIF_NAME=nsgif -DNSGIF_VERSION=1.0.0 '-I$(TP)/netsurf/libnsgif/include' '-I$(TP)/netsurf/libnsgif/src' -Wall -Wextra -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsgif/src_lzw.o: $(TP)/netsurf/libnsgif/src/lzw.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsgif] $<"
	@$(CC) $(NS_CFLAGS) -DNSGIF_NAME=nsgif -DNSGIF_VERSION=1.0.0 '-I$(TP)/netsurf/libnsgif/include' '-I$(TP)/netsurf/libnsgif/src' -Wall -Wextra -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsbmp/src_libnsbmp.o: $(TP)/netsurf/libnsbmp/src/libnsbmp.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsbmp] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libnsbmp/include' '-I$(TP)/netsurf/libnsbmp/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnspsl/src_nspsl.o: $(TP)/netsurf/libnspsl/src/nspsl.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnspsl] $<"
	@$(CC) $(NS_CFLAGS) -D_GNU_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libnspsl/include' '-I$(TP)/netsurf/libnspsl/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 -D_POSIX_C_SOURCE=200809L $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnslog/build-release-x86_64-myos-release-lib-static_filter-parser.o: $(TP)/netsurf/libnslog/gen/filter-parser.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnslog] $<"
	@$(CC) $(NS_CFLAGS) -D_GNU_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libnslog/include' '-I$(TP)/netsurf/libnslog/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 -D_POSIX_C_SOURCE=200809L '-I$(TP)/netsurf/libnslog/gen/.' '-I$(TP)/netsurf/libnslog/src' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnslog/build-release-x86_64-myos-release-lib-static_filter-lexer.o: $(TP)/netsurf/libnslog/gen/filter-lexer.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnslog] $<"
	@$(CC) $(NS_CFLAGS) -D_GNU_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libnslog/include' '-I$(TP)/netsurf/libnslog/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 -D_POSIX_C_SOURCE=200809L '-I$(TP)/netsurf/libnslog/gen/.' '-I$(TP)/netsurf/libnslog/src' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnslog/src_core.o: $(TP)/netsurf/libnslog/src/core.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnslog] $<"
	@$(CC) $(NS_CFLAGS) -D_GNU_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libnslog/include' '-I$(TP)/netsurf/libnslog/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 -D_POSIX_C_SOURCE=200809L '-I$(TP)/netsurf/libnslog/gen/.' '-I$(TP)/netsurf/libnslog/src' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnslog/src_filter.o: $(TP)/netsurf/libnslog/src/filter.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnslog] $<"
	@$(CC) $(NS_CFLAGS) -D_GNU_SOURCE -D_DEFAULT_SOURCE '-I$(TP)/netsurf/libnslog/include' '-I$(TP)/netsurf/libnslog/src' -Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 -D_POSIX_C_SOURCE=200809L '-I$(TP)/netsurf/libnslog/gen/.' '-I$(TP)/netsurf/libnslog/src' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_libnsfb.o: $(TP)/netsurf/libnsfb/src/libnsfb.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_dump.o: $(TP)/netsurf/libnsfb/src/dump.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_cursor.o: $(TP)/netsurf/libnsfb/src/cursor.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_palette.o: $(TP)/netsurf/libnsfb/src/palette.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_plot_api.o: $(TP)/netsurf/libnsfb/src/plot/api.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_plot_util.o: $(TP)/netsurf/libnsfb/src/plot/util.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_plot_generic.o: $(TP)/netsurf/libnsfb/src/plot/generic.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_plot_32bpp-xrgb8888.o: $(TP)/netsurf/libnsfb/src/plot/32bpp-xrgb8888.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_plot_32bpp-xbgr8888.o: $(TP)/netsurf/libnsfb/src/plot/32bpp-xbgr8888.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_plot_16bpp.o: $(TP)/netsurf/libnsfb/src/plot/16bpp.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_plot_8bpp.o: $(TP)/netsurf/libnsfb/src/plot/8bpp.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_surface_surface.o: $(TP)/netsurf/libnsfb/src/surface/surface.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_surface_ram.o: $(TP)/netsurf/libnsfb/src/surface/ram.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/libnsfb/src_surface_myos.o: $(TP)/netsurf/libnsfb/src/surface/myos.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libnsfb] $<"
	@$(CC) $(NS_CFLAGS) -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L '-I$(TP)/netsurf/libnsfb/include' '-I$(TP)/netsurf/libnsfb/src' -Wall -Wextra -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs '-D_ALIGNED=__attribute__((aligned))' -DSTMTEXPR=1 -DNDEBUG -std=c99 $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_content.o: $(TP)/netsurf/netsurf/content/content.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_content_factory.o: $(TP)/netsurf/netsurf/content/content_factory.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetch.o: $(TP)/netsurf/netsurf/content/fetch.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_about.o: $(TP)/netsurf/netsurf/content/fetchers/about/about.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_blank.o: $(TP)/netsurf/netsurf/content/fetchers/about/blank.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_certificate.o: $(TP)/netsurf/netsurf/content/fetchers/about/certificate.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_chart.o: $(TP)/netsurf/netsurf/content/fetchers/about/chart.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_choices.o: $(TP)/netsurf/netsurf/content/fetchers/about/choices.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_config.o: $(TP)/netsurf/netsurf/content/fetchers/about/config.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_imagecache.o: $(TP)/netsurf/netsurf/content/fetchers/about/imagecache.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_nscolours.o: $(TP)/netsurf/netsurf/content/fetchers/about/nscolours.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_query_auth.o: $(TP)/netsurf/netsurf/content/fetchers/about/query_auth.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_query.o: $(TP)/netsurf/netsurf/content/fetchers/about/query.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_query_fetcherror.o: $(TP)/netsurf/netsurf/content/fetchers/about/query_fetcherror.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_query_privacy.o: $(TP)/netsurf/netsurf/content/fetchers/about/query_privacy.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_query_timeout.o: $(TP)/netsurf/netsurf/content/fetchers/about/query_timeout.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_testament.o: $(TP)/netsurf/netsurf/content/fetchers/about/testament.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_about_websearch.o: $(TP)/netsurf/netsurf/content/fetchers/about/websearch.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_curl.o: $(TP)/netsurf/netsurf/content/fetchers/curl.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_file_dirlist.o: $(TP)/netsurf/netsurf/content/fetchers/file/dirlist.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_data.o: $(TP)/netsurf/netsurf/content/fetchers/data.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_file_file.o: $(TP)/netsurf/netsurf/content/fetchers/file/file.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_fetchers_resource.o: $(TP)/netsurf/netsurf/content/fetchers/resource.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_css_css.o: $(TP)/netsurf/netsurf/content/handlers/css/css.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_css_dump.o: $(TP)/netsurf/netsurf/content/handlers/css/dump.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_css_hints.o: $(TP)/netsurf/netsurf/content/handlers/css/hints.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_css_internal.o: $(TP)/netsurf/netsurf/content/handlers/css/internal.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_css_select.o: $(TP)/netsurf/netsurf/content/handlers/css/select.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_box_construct.o: $(TP)/netsurf/netsurf/content/handlers/html/box_construct.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_box_inspect.o: $(TP)/netsurf/netsurf/content/handlers/html/box_inspect.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_box_manipulate.o: $(TP)/netsurf/netsurf/content/handlers/html/box_manipulate.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_box_normalise.o: $(TP)/netsurf/netsurf/content/handlers/html/box_normalise.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_box_textarea.o: $(TP)/netsurf/netsurf/content/handlers/html/box_textarea.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_box_special.o: $(TP)/netsurf/netsurf/content/handlers/html/box_special.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_css.o: $(TP)/netsurf/netsurf/content/handlers/html/css.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_css_fetcher.o: $(TP)/netsurf/netsurf/content/handlers/html/css_fetcher.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_dom_event.o: $(TP)/netsurf/netsurf/content/handlers/html/dom_event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_font.o: $(TP)/netsurf/netsurf/content/handlers/html/font.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_form.o: $(TP)/netsurf/netsurf/content/handlers/html/form.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_forms.o: $(TP)/netsurf/netsurf/content/handlers/html/forms.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_html.o: $(TP)/netsurf/netsurf/content/handlers/html/html.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_imagemap.o: $(TP)/netsurf/netsurf/content/handlers/html/imagemap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_interaction.o: $(TP)/netsurf/netsurf/content/handlers/html/interaction.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_layout.o: $(TP)/netsurf/netsurf/content/handlers/html/layout.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_layout_flex.o: $(TP)/netsurf/netsurf/content/handlers/html/layout_flex.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_object.o: $(TP)/netsurf/netsurf/content/handlers/html/object.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_redraw.o: $(TP)/netsurf/netsurf/content/handlers/html/redraw.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_redraw_border.o: $(TP)/netsurf/netsurf/content/handlers/html/redraw_border.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_script.o: $(TP)/netsurf/netsurf/content/handlers/html/script.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_table.o: $(TP)/netsurf/netsurf/content/handlers/html/table.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_html_textselection.o: $(TP)/netsurf/netsurf/content/handlers/html/textselection.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_image_bmp.o: $(TP)/netsurf/netsurf/content/handlers/image/bmp.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_image_gif.o: $(TP)/netsurf/netsurf/content/handlers/image/gif.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_image_ico.o: $(TP)/netsurf/netsurf/content/handlers/image/ico.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_image_image.o: $(TP)/netsurf/netsurf/content/handlers/image/image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_image_image_cache.o: $(TP)/netsurf/netsurf/content/handlers/image/image_cache.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_image_jpeg.o: $(TP)/netsurf/netsurf/content/handlers/image/jpeg.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_image_png.o: $(TP)/netsurf/netsurf/content/handlers/image/png.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_javascript_fetcher.o: $(TP)/netsurf/netsurf/content/handlers/javascript/fetcher.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_javascript_none_none.o: $(TP)/netsurf/netsurf/content/handlers/javascript/none/none.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_handlers_text_textplain.o: $(TP)/netsurf/netsurf/content/handlers/text/textplain.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_hlcache.o: $(TP)/netsurf/netsurf/content/hlcache.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_llcache.o: $(TP)/netsurf/netsurf/content/llcache.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_mimesniff.o: $(TP)/netsurf/netsurf/content/mimesniff.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_no_backing_store.o: $(TP)/netsurf/netsurf/content/no_backing_store.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_textsearch.o: $(TP)/netsurf/netsurf/content/textsearch.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/content_urldb.o: $(TP)/netsurf/netsurf/content/urldb.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_bitmap.o: $(TP)/netsurf/netsurf/desktop/bitmap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_browser.o: $(TP)/netsurf/netsurf/desktop/browser.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_browser_history.o: $(TP)/netsurf/netsurf/desktop/browser_history.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_browser_window.o: $(TP)/netsurf/netsurf/desktop/browser_window.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_cookie_manager.o: $(TP)/netsurf/netsurf/desktop/cookie_manager.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_cw_helper.o: $(TP)/netsurf/netsurf/desktop/cw_helper.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_download.o: $(TP)/netsurf/netsurf/desktop/download.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_font_haru.o: $(TP)/netsurf/netsurf/desktop/font_haru.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_frames.o: $(TP)/netsurf/netsurf/desktop/frames.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_global_history.o: $(TP)/netsurf/netsurf/desktop/global_history.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_hotlist.o: $(TP)/netsurf/netsurf/desktop/hotlist.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_gui_factory.o: $(TP)/netsurf/netsurf/desktop/gui_factory.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_knockout.o: $(TP)/netsurf/netsurf/desktop/knockout.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_local_history.o: $(TP)/netsurf/netsurf/desktop/local_history.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_mouse.o: $(TP)/netsurf/netsurf/desktop/mouse.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_netsurf.o: $(TP)/netsurf/netsurf/desktop/netsurf.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_page-info.o: $(TP)/netsurf/netsurf/desktop/page-info.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_plot_style.o: $(TP)/netsurf/netsurf/desktop/plot_style.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_print.o: $(TP)/netsurf/netsurf/desktop/print.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_save_complete.o: $(TP)/netsurf/netsurf/desktop/save_complete.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_save_pdf.o: $(TP)/netsurf/netsurf/desktop/save_pdf.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_scrollbar.o: $(TP)/netsurf/netsurf/desktop/scrollbar.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_save_text.o: $(TP)/netsurf/netsurf/desktop/save_text.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_search.o: $(TP)/netsurf/netsurf/desktop/search.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_searchweb.o: $(TP)/netsurf/netsurf/desktop/searchweb.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_selection.o: $(TP)/netsurf/netsurf/desktop/selection.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_system_colour.o: $(TP)/netsurf/netsurf/desktop/system_colour.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_textarea.o: $(TP)/netsurf/netsurf/desktop/textarea.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_textinput.o: $(TP)/netsurf/netsurf/desktop/textinput.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_treeview.o: $(TP)/netsurf/netsurf/desktop/treeview.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/desktop_version.o: $(TP)/netsurf/netsurf/desktop/version.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_bitmap.o: $(TP)/netsurf/netsurf/frontends/framebuffer/bitmap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_clipboard.o: $(TP)/netsurf/netsurf/frontends/framebuffer/clipboard.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_corewindow.o: $(TP)/netsurf/netsurf/frontends/framebuffer/corewindow.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_bitmap.o: $(TP)/netsurf/netsurf/frontends/framebuffer/fbtk/bitmap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_event.o: $(TP)/netsurf/netsurf/frontends/framebuffer/fbtk/event.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_fbtk.o: $(TP)/netsurf/netsurf/frontends/framebuffer/fbtk/fbtk.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_fill.o: $(TP)/netsurf/netsurf/frontends/framebuffer/fbtk/fill.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_osk.o: $(TP)/netsurf/netsurf/frontends/framebuffer/fbtk/osk.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_scroll.o: $(TP)/netsurf/netsurf/frontends/framebuffer/fbtk/scroll.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_text.o: $(TP)/netsurf/netsurf/frontends/framebuffer/fbtk/text.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_user.o: $(TP)/netsurf/netsurf/frontends/framebuffer/fbtk/user.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_fbtk_window.o: $(TP)/netsurf/netsurf/frontends/framebuffer/fbtk/window.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_fetch.o: $(TP)/netsurf/netsurf/frontends/framebuffer/fetch.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_findfile.o: $(TP)/netsurf/netsurf/frontends/framebuffer/findfile.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_font_freetype.o: $(TP)/netsurf/netsurf/frontends/framebuffer/font_freetype.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_framebuffer.o: $(TP)/netsurf/netsurf/frontends/framebuffer/framebuffer.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_gui.o: $(TP)/netsurf/netsurf/frontends/framebuffer/gui.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_local_history.o: $(TP)/netsurf/netsurf/frontends/framebuffer/local_history.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/frontends_framebuffer_schedule.o: $(TP)/netsurf/netsurf/frontends/framebuffer/schedule.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_bloom.o: $(TP)/netsurf/netsurf/utils/bloom.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_corestrings.o: $(TP)/netsurf/netsurf/utils/corestrings.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_file.o: $(TP)/netsurf/netsurf/utils/file.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_filepath.o: $(TP)/netsurf/netsurf/utils/filepath.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_hashmap.o: $(TP)/netsurf/netsurf/utils/hashmap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_hashtable.o: $(TP)/netsurf/netsurf/utils/hashtable.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_http_cache-control.o: $(TP)/netsurf/netsurf/utils/http/cache-control.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_http_challenge.o: $(TP)/netsurf/netsurf/utils/http/challenge.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_http_content-disposition.o: $(TP)/netsurf/netsurf/utils/http/content-disposition.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_http_content-type.o: $(TP)/netsurf/netsurf/utils/http/content-type.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_http_generics.o: $(TP)/netsurf/netsurf/utils/http/generics.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_http_parameter.o: $(TP)/netsurf/netsurf/utils/http/parameter.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_http_primitives.o: $(TP)/netsurf/netsurf/utils/http/primitives.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_http_strict-transport-security.o: $(TP)/netsurf/netsurf/utils/http/strict-transport-security.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_http_www-authenticate.o: $(TP)/netsurf/netsurf/utils/http/www-authenticate.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_idna.o: $(TP)/netsurf/netsurf/utils/idna.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_libdom.o: $(TP)/netsurf/netsurf/utils/libdom.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_log.o: $(TP)/netsurf/netsurf/utils/log.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_messages.o: $(TP)/netsurf/netsurf/utils/messages.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_nscolour.o: $(TP)/netsurf/netsurf/utils/nscolour.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_nsurl_nsurl.o: $(TP)/netsurf/netsurf/utils/nsurl/nsurl.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_nsoption.o: $(TP)/netsurf/netsurf/utils/nsoption.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_nsurl_parse.o: $(TP)/netsurf/netsurf/utils/nsurl/parse.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_punycode.o: $(TP)/netsurf/netsurf/utils/punycode.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_ssl_certs.o: $(TP)/netsurf/netsurf/utils/ssl_certs.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_talloc.o: $(TP)/netsurf/netsurf/utils/talloc.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_time.o: $(TP)/netsurf/netsurf/utils/time.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_url.o: $(TP)/netsurf/netsurf/utils/url.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_useragent.o: $(TP)/netsurf/netsurf/utils/useragent.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_utf8.o: $(TP)/netsurf/netsurf/utils/utf8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/utils_utils.o: $(TP)/netsurf/netsurf/utils/utils.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-caret_image.o: $(TP)/netsurf/netsurf/gen/image-caret_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-hand_image.o: $(TP)/netsurf/netsurf/gen/image-hand_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-history_image.o: $(TP)/netsurf/netsurf/gen/image-history_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-history_image_g.o: $(TP)/netsurf/netsurf/gen/image-history_image_g.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-left_arrow.o: $(TP)/netsurf/netsurf/gen/image-left_arrow.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-left_arrow_g.o: $(TP)/netsurf/netsurf/gen/image-left_arrow_g.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-move_image.o: $(TP)/netsurf/netsurf/gen/image-move_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-menu_image.o: $(TP)/netsurf/netsurf/gen/image-menu_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-osk_image.o: $(TP)/netsurf/netsurf/gen/image-osk_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-pointer_image.o: $(TP)/netsurf/netsurf/gen/image-pointer_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-progress_image.o: $(TP)/netsurf/netsurf/gen/image-progress_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-reload.o: $(TP)/netsurf/netsurf/gen/image-reload.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-right_arrow_g.o: $(TP)/netsurf/netsurf/gen/image-right_arrow_g.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-reload_g.o: $(TP)/netsurf/netsurf/gen/image-reload_g.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-right_arrow.o: $(TP)/netsurf/netsurf/gen/image-right_arrow.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-scrolld.o: $(TP)/netsurf/netsurf/gen/image-scrolld.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-scrolll.o: $(TP)/netsurf/netsurf/gen/image-scrolll.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-scrollr.o: $(TP)/netsurf/netsurf/gen/image-scrollr.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-scrollu.o: $(TP)/netsurf/netsurf/gen/image-scrollu.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-stop_image.o: $(TP)/netsurf/netsurf/gen/image-stop_image.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-stop_image_g.o: $(TP)/netsurf/netsurf/gen/image-stop_image_g.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber0.o: $(TP)/netsurf/netsurf/gen/image-throbber0.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber1.o: $(TP)/netsurf/netsurf/gen/image-throbber1.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber2.o: $(TP)/netsurf/netsurf/gen/image-throbber2.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber3.o: $(TP)/netsurf/netsurf/gen/image-throbber3.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber4.o: $(TP)/netsurf/netsurf/gen/image-throbber4.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber6.o: $(TP)/netsurf/netsurf/gen/image-throbber6.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber5.o: $(TP)/netsurf/netsurf/gen/image-throbber5.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber7.o: $(TP)/netsurf/netsurf/gen/image-throbber7.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/netsurf/build_x86_64-myos-framebuffer_image-throbber8.o: $(TP)/netsurf/netsurf/gen/image-throbber8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [netsurf] $<"
	@$(CC) $(NS_CFLAGS) -W -Wall -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations -Wuninitialized -Wno-unused-parameter -Wno-unused-but-set-variable -Wimplicit-fallthrough=5 -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs '-I$(TP)/netsurf/netsurf/.' '-I$(TP)/netsurf/netsurf/include' '-I$(TP)/netsurf/netsurf/gen/.' '-I$(TP)/netsurf/netsurf/frontends' '-I$(TP)/netsurf/netsurf/content/handlers' -DWITH_JPEG -UWITH_PDF_EXPORT -ULIBICONV_PLUG -DCURL_STATICLIB -DWITH_CURL -DUTF8PROC_STATIC -DWITH_UTF8PROC -DWITH_PNG -DWITH_BMP -DWITH_GIF -DWITH_NSPSL -DWITH_NSLOG '-DNETSURF_UA_FORMAT_STRING="Mozilla/5.0 (%s) NetSurf/%d.%d"' '-DNETSURF_HOMEPAGE="about:welcome"' -DNETSURF_LOG_LEVEL=VERBOSE '-DNETSURF_BUILTIN_LOG_FILTER="(level:WARNING || cat:jserrors)"' '-DNETSURF_BUILTIN_VERBOSE_FILTER="(level:VERBOSE || cat:jserrors)"' -DSTMTEXPR=1 -std=c99 -Dnsframebuffer -Dsmall '-DNETSURF_FB_RESPATH="/embed"' '-DNETSURF_FB_FONTPATH="/embed/fonts"' '-DNETSURF_FB_FONT_SANS_SERIF="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="DejaVuSans.ttf"' '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="DejaVuSans-Bold.ttf"' '-DNETSURF_FB_FONT_SERIF="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_SERIF_BOLD="DejaVuSerif-Bold.ttf"' '-DNETSURF_FB_FONT_MONOSPACE="DejaVuSansMono.ttf"' '-DNETSURF_FB_FONT_MONOSPACE_BOLD="DejaVuSansMono-Bold.ttf"' '-DNETSURF_FB_FONT_CURSIVE="DejaVuSerif.ttf"' '-DNETSURF_FB_FONT_FANTASY="DejaVuSans.ttf"' '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' -DFB_USE_FREETYPE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_BSD_SOURCE -D_DEFAULT_SOURCE -D_NETBSD_SOURCE $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcapimin.c.o: $(TP)/libjpeg-turbo/src/jcapimin.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcapistd-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcapistd-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcapistd-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcapistd-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcapistd-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcapistd-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jccoefct-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jccoefct-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jccoefct-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jccoefct-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jccolor-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jccolor-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jccolor-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jccolor-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jccolor-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jccolor-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcdctmgr-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcdctmgr-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcdctmgr-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcdctmgr-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcdiffct-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcdiffct-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcdiffct-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcdiffct-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcdiffct-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcdiffct-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jchuff.c.o: $(TP)/libjpeg-turbo/src/jchuff.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcicc.c.o: $(TP)/libjpeg-turbo/src/jcicc.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcinit.c.o: $(TP)/libjpeg-turbo/src/jcinit.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jclhuff.c.o: $(TP)/libjpeg-turbo/src/jclhuff.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jclossls-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jclossls-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jclossls-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jclossls-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jclossls-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jclossls-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcmainct-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcmainct-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcmainct-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcmainct-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcmainct-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcmainct-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcmarker.c.o: $(TP)/libjpeg-turbo/src/jcmarker.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcmaster.c.o: $(TP)/libjpeg-turbo/src/jcmaster.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcomapi.c.o: $(TP)/libjpeg-turbo/src/jcomapi.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcparam.c.o: $(TP)/libjpeg-turbo/src/jcparam.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcphuff.c.o: $(TP)/libjpeg-turbo/src/jcphuff.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcprepct-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcprepct-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcprepct-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcprepct-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcprepct-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcprepct-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcsample-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcsample-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcsample-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcsample-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcsample-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jcsample-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jctrans.c.o: $(TP)/libjpeg-turbo/src/jctrans.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdapimin.c.o: $(TP)/libjpeg-turbo/src/jdapimin.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdapistd-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdapistd-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdapistd-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdapistd-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdapistd-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdapistd-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdatadst.c.o: $(TP)/libjpeg-turbo/src/jdatadst.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdatasrc.c.o: $(TP)/libjpeg-turbo/src/jdatasrc.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdcoefct-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdcoefct-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdcoefct-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdcoefct-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdcolor-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdcolor-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdcolor-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdcolor-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdcolor-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdcolor-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jddctmgr-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jddctmgr-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jddctmgr-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jddctmgr-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jddiffct-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jddiffct-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jddiffct-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jddiffct-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jddiffct-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jddiffct-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdhuff.c.o: $(TP)/libjpeg-turbo/src/jdhuff.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdicc.c.o: $(TP)/libjpeg-turbo/src/jdicc.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdinput.c.o: $(TP)/libjpeg-turbo/src/jdinput.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdlhuff.c.o: $(TP)/libjpeg-turbo/src/jdlhuff.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdlossls-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdlossls-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdlossls-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdlossls-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdlossls-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdlossls-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdmainct-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdmainct-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdmainct-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdmainct-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdmainct-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdmainct-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdmarker.c.o: $(TP)/libjpeg-turbo/src/jdmarker.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdmaster.c.o: $(TP)/libjpeg-turbo/src/jdmaster.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdmerge-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdmerge-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdmerge-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdmerge-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdphuff.c.o: $(TP)/libjpeg-turbo/src/jdphuff.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdpostct-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdpostct-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdpostct-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdpostct-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdpostct-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdpostct-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdsample-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdsample-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdsample-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdsample-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdsample-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jdsample-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdtrans.c.o: $(TP)/libjpeg-turbo/src/jdtrans.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jerror.c.o: $(TP)/libjpeg-turbo/src/jerror.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jfdctflt.c.o: $(TP)/libjpeg-turbo/src/jfdctflt.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jfdctfst-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jfdctfst-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jfdctfst-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jfdctfst-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jfdctint-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jfdctint-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jfdctint-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jfdctint-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jidctflt-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jidctflt-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jidctflt-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jidctflt-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jidctfst-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jidctfst-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jidctfst-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jidctfst-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jidctint-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jidctint-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jidctint-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jidctint-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jidctred-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jidctred-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jidctred-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jidctred-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jmemmgr.c.o: $(TP)/libjpeg-turbo/src/jmemmgr.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jmemnobs.c.o: $(TP)/libjpeg-turbo/src/jmemnobs.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jpeg_nbits.c.o: $(TP)/libjpeg-turbo/src/jpeg_nbits.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jquant1-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jquant1-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jquant1-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jquant1-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jquant2-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jquant2-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jquant2-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jquant2-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jutils-8.c.o: $(TP)/libjpeg-turbo/src/wrapper/jutils-8.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jutils-12.c.o: $(TP)/libjpeg-turbo/src/wrapper/jutils-12.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jutils-16.c.o: $(TP)/libjpeg-turbo/src/wrapper/jutils-16.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jaricom.c.o: $(TP)/libjpeg-turbo/src/jaricom.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jcarith.c.o: $(TP)/libjpeg-turbo/src/jcarith.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/libjpeg/jdarith.c.o: $(TP)/libjpeg-turbo/src/jdarith.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libjpeg] $<"
	@$(CC) $(NS_CFLAGS) '-I$(TP)/libjpeg-turbo/myos/.' '-I$(TP)/libjpeg-turbo/src' -Iuser/posix/include -DNDEBUG $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/autofit.c.o: $(TP)/freetype/src/autofit/autofit.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftbase.c.o: $(TP)/freetype/src/base/ftbase.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftbbox.c.o: $(TP)/freetype/src/base/ftbbox.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftbdf.c.o: $(TP)/freetype/src/base/ftbdf.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftbitmap.c.o: $(TP)/freetype/src/base/ftbitmap.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftcid.c.o: $(TP)/freetype/src/base/ftcid.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftfstype.c.o: $(TP)/freetype/src/base/ftfstype.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftgasp.c.o: $(TP)/freetype/src/base/ftgasp.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftglyph.c.o: $(TP)/freetype/src/base/ftglyph.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftgxval.c.o: $(TP)/freetype/src/base/ftgxval.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftinit.c.o: $(TP)/freetype/src/base/ftinit.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftmm.c.o: $(TP)/freetype/src/base/ftmm.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftotval.c.o: $(TP)/freetype/src/base/ftotval.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftpatent.c.o: $(TP)/freetype/src/base/ftpatent.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftpfr.c.o: $(TP)/freetype/src/base/ftpfr.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftstroke.c.o: $(TP)/freetype/src/base/ftstroke.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftsynth.c.o: $(TP)/freetype/src/base/ftsynth.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/fttype1.c.o: $(TP)/freetype/src/base/fttype1.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftwinfnt.c.o: $(TP)/freetype/src/base/ftwinfnt.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/bdf.c.o: $(TP)/freetype/src/bdf/bdf.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftbzip2.c.o: $(TP)/freetype/src/bzip2/ftbzip2.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftcache.c.o: $(TP)/freetype/src/cache/ftcache.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/cff.c.o: $(TP)/freetype/src/cff/cff.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/type1cid.c.o: $(TP)/freetype/src/cid/type1cid.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftgzip.c.o: $(TP)/freetype/src/gzip/ftgzip.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftlzw.c.o: $(TP)/freetype/src/lzw/ftlzw.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/pcf.c.o: $(TP)/freetype/src/pcf/pcf.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/pfr.c.o: $(TP)/freetype/src/pfr/pfr.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/psaux.c.o: $(TP)/freetype/src/psaux/psaux.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/pshinter.c.o: $(TP)/freetype/src/pshinter/pshinter.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/psnames.c.o: $(TP)/freetype/src/psnames/psnames.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/raster.c.o: $(TP)/freetype/src/raster/raster.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/sdf.c.o: $(TP)/freetype/src/sdf/sdf.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/sfnt.c.o: $(TP)/freetype/src/sfnt/sfnt.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/smooth.c.o: $(TP)/freetype/src/smooth/smooth.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/svg.c.o: $(TP)/freetype/src/svg/svg.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/truetype.c.o: $(TP)/freetype/src/truetype/truetype.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/type1.c.o: $(TP)/freetype/src/type1/type1.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/type42.c.o: $(TP)/freetype/src/type42/type42.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/winfnt.c.o: $(TP)/freetype/src/winfonts/winfnt.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftsystem.c.o: $(TP)/freetype/builds/unix/ftsystem.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/freetype/ftdebug.c.o: $(TP)/freetype/src/base/ftdebug.c
	@mkdir -p $(dir $@)
	@echo "  CC  [freetype] $<"
	@$(CC) $(NS_CFLAGS) -DFT2_BUILD_LIBRARY '-I$(TP)/freetype/myos/include' '-I$(TP)/freetype/include' '-I$(TP)/freetype/myos/include/freetype/config' -Iuser/posix/include -DNDEBUG -fvisibility=hidden $(NS_INC) -c $< -o $@

$(NS_OBJ)/utf8proc/utf8proc.o: $(TP)/utf8proc/utf8proc.c
	@mkdir -p $(dir $@)
	@echo "  CC  [utf8proc] $<"
	@$(CC) $(NS_CFLAGS) -DUTF8PROC_STATIC $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/png.o: $(TP)/libpng/png.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngerror.o: $(TP)/libpng/pngerror.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngget.o: $(TP)/libpng/pngget.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngmem.o: $(TP)/libpng/pngmem.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngpread.o: $(TP)/libpng/pngpread.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngread.o: $(TP)/libpng/pngread.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngrio.o: $(TP)/libpng/pngrio.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngrtran.o: $(TP)/libpng/pngrtran.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngrutil.o: $(TP)/libpng/pngrutil.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngset.o: $(TP)/libpng/pngset.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngtrans.o: $(TP)/libpng/pngtrans.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngwio.o: $(TP)/libpng/pngwio.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngwrite.o: $(TP)/libpng/pngwrite.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngwtran.o: $(TP)/libpng/pngwtran.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

$(NS_OBJ)/libpng/pngwutil.o: $(TP)/libpng/pngwutil.c
	@mkdir -p $(dir $@)
	@echo "  CC  [libpng] $<"
	@$(CC) $(NS_CFLAGS) -DPNG_INTEL_SSE_OPT=0 -DPNG_ARM_NEON_OPT=0 '-I$(TP)/libpng/.' $(NS_INC) -c $< -o $@

