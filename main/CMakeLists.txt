
#
# Main component of TF Micro project 'person_detection'.
#

idf_component_register(
    SRCS
        "detection_responder.cc"
        "image_provider.cc"
        "main.cc"
        "main_functions.cc"
        "model_settings.cc"
        "person_detect_model_data.cc"
        "app_camera_esp.c"
        "esp_cli.c"
        "audio_provider.cc"
        "feature_provider.cc"
        "no_micro_features_data.cc"
        "yes_micro_features_data.cc"
        "model.cc"
        "recognize_commands.cc"
        "command_responder.cc"
        "micro_features_generator.cc"
        "ringbuf.c"

    PRIV_REQUIRES console static_images spi_flash display_manager driver esp_timer
    INCLUDE_DIRS "")
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-maybe-uninitialized
    -Wno-missing-field-initializers
    -Wno-error=sign-compare
    -Wno-error=double-promotion
    -Wno-type-limits)