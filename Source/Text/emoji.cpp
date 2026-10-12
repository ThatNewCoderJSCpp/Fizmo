#include "fizmo_library.hpp"
#include "emoji.hpp"
#include "../Images/Formats/image_formats.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>

namespace fizmo {
namespace text {

namespace {

struct Shortcode {
    const char* name;
    const char* utf8;
};

const Shortcode kShortcodes[] = {
    { "+1", "\xF0\x9F\x91\x8D" },
    { "-1", "\xF0\x9F\x91\x8E" },
    { "100", "\xF0\x9F\x92\xAF" },
    { "airplane", "\xE2\x9C\x88" },
    { "alarm_clock", "\xE2\x8F\xB0" },
    { "alien", "\xF0\x9F\x91\xBD" },
    { "anger", "\xF0\x9F\x92\xA2" },
    { "angry", "\xF0\x9F\x98\xA0" },
    { "apple", "\xF0\x9F\x8D\x8E" },
    { "arrow_down", "\xE2\xAC\x87" },
    { "arrow_left", "\xE2\xAC\x85" },
    { "arrow_right", "\xE2\x9E\xA1" },
    { "arrow_up", "\xE2\xAC\x86" },
    { "arrows_counterclockwise", "\xF0\x9F\x94\x84" },
    { "art", "\xF0\x9F\x8E\xA8" },
    { "astonished", "\xF0\x9F\x98\xB2" },
    { "avocado", "\xF0\x9F\xA5\x91" },
    { "baby", "\xF0\x9F\x91\xB6" },
    { "bacon", "\xF0\x9F\xA5\x93" },
    { "balloon", "\xF0\x9F\x8E\x88" },
    { "ballot_box_with_check", "\xE2\x98\x91" },
    { "banana", "\xF0\x9F\x8D\x8C" },
    { "bangbang", "\xE2\x80\xBC" },
    { "bar_chart", "\xF0\x9F\x93\x8A" },
    { "baseball", "\xE2\x9A\xBE" },
    { "basketball", "\xF0\x9F\x8F\x80" },
    { "battery", "\xF0\x9F\x94\x8B" },
    { "beach", "\xF0\x9F\x8F\x96" },
    { "bear", "\xF0\x9F\x90\xBB" },
    { "bee", "\xF0\x9F\x90\x9D" },
    { "beer", "\xF0\x9F\x8D\xBA" },
    { "beers", "\xF0\x9F\x8D\xBB" },
    { "bell", "\xF0\x9F\x94\x94" },
    { "bike", "\xF0\x9F\x9A\xB2" },
    { "bird", "\xF0\x9F\x90\xA6" },
    { "birthday", "\xF0\x9F\x8E\x82" },
    { "black_circle", "\xE2\x9A\xAB" },
    { "black_heart", "\xF0\x9F\x96\xA4" },
    { "blue_circle", "\xF0\x9F\x94\xB5" },
    { "blue_heart", "\xF0\x9F\x92\x99" },
    { "blue_square", "\xF0\x9F\x9F\xA6" },
    { "blush", "\xF0\x9F\x98\x8A" },
    { "bomb", "\xF0\x9F\x92\xA3" },
    { "book", "\xF0\x9F\x93\x96" },
    { "bookmark", "\xF0\x9F\x94\x96" },
    { "books", "\xF0\x9F\x93\x9A" },
    { "boom", "\xF0\x9F\x92\xA5" },
    { "brain", "\xF0\x9F\xA7\xA0" },
    { "bread", "\xF0\x9F\x8D\x9E" },
    { "broken_heart", "\xF0\x9F\x92\x94" },
    { "brown_heart", "\xF0\x9F\xA4\x8E" },
    { "bug", "\xF0\x9F\x90\x9B" },
    { "bulb", "\xF0\x9F\x92\xA1" },
    { "burrito", "\xF0\x9F\x8C\xAF" },
    { "bust_in_silhouette", "\xF0\x9F\x91\xA4" },
    { "busts_in_silhouette", "\xF0\x9F\x91\xA5" },
    { "butterfly", "\xF0\x9F\xA6\x8B" },
    { "cactus", "\xF0\x9F\x8C\xB5" },
    { "cake", "\xF0\x9F\x8D\xB0" },
    { "calendar", "\xF0\x9F\x93\x85" },
    { "call_me", "\xF0\x9F\xA4\x99" },
    { "camera", "\xF0\x9F\x93\xB7" },
    { "candy", "\xF0\x9F\x8D\xAC" },
    { "car", "\xF0\x9F\x9A\x97" },
    { "carrot", "\xF0\x9F\xA5\x95" },
    { "cat", "\xF0\x9F\x90\xB1" },
    { "champagne", "\xF0\x9F\x8D\xBE" },
    { "chart", "\xF0\x9F\x92\xB9" },
    { "chart_with_downwards_trend", "\xF0\x9F\x93\x89" },
    { "chart_with_upwards_trend", "\xF0\x9F\x93\x88" },
    { "checkered_flag", "\xF0\x9F\x8F\x81" },
    { "cheese", "\xF0\x9F\xA7\x80" },
    { "cherries", "\xF0\x9F\x8D\x92" },
    { "cherry_blossom", "\xF0\x9F\x8C\xB8" },
    { "chicken", "\xF0\x9F\x90\x94" },
    { "chocolate_bar", "\xF0\x9F\x8D\xAB" },
    { "christmas_tree", "\xF0\x9F\x8E\x84" },
    { "cinema", "\xF0\x9F\x8E\xA6" },
    { "clap", "\xF0\x9F\x91\x8F" },
    { "clapper", "\xF0\x9F\x8E\xAC" },
    { "clipboard", "\xF0\x9F\x93\x8B" },
    { "cloud", "\xE2\x98\x81" },
    { "cloud_with_rain", "\xF0\x9F\x8C\xA7" },
    { "clown", "\xF0\x9F\xA4\xA1" },
    { "cocktail", "\xF0\x9F\x8D\xB8" },
    { "coffee", "\xE2\x98\x95" },
    { "cold_face", "\xF0\x9F\xA5\xB6" },
    { "comet", "\xE2\x98\x84" },
    { "compass", "\xF0\x9F\xA7\xAD" },
    { "computer", "\xF0\x9F\x92\xBB" },
    { "confetti_ball", "\xF0\x9F\x8E\x8A" },
    { "confounded", "\xF0\x9F\x98\x96" },
    { "confused", "\xF0\x9F\x98\x95" },
    { "cookie", "\xF0\x9F\x8D\xAA" },
    { "cool", "\xF0\x9F\x86\x92" },
    { "copyright", "\xC2\xA9" },
    { "corn", "\xF0\x9F\x8C\xBD" },
    { "cow", "\xF0\x9F\x90\xAE" },
    { "cowboy", "\xF0\x9F\xA4\xA0" },
    { "crab", "\xF0\x9F\xA6\x80" },
    { "credit_card", "\xF0\x9F\x92\xB3" },
    { "crossed_fingers", "\xF0\x9F\xA4\x9E" },
    { "crown", "\xF0\x9F\x91\x91" },
    { "cry", "\xF0\x9F\x98\xA2" },
    { "crystal_ball", "\xF0\x9F\x94\xAE" },
    { "cupid", "\xF0\x9F\x92\x98" },
    { "cursing_face", "\xF0\x9F\xA4\xAC" },
    { "dancer", "\xF0\x9F\x92\x83" },
    { "dart", "\xF0\x9F\x8E\xAF" },
    { "dash", "\xF0\x9F\x92\xA8" },
    { "desert", "\xF0\x9F\x8F\x9C" },
    { "detective", "\xF0\x9F\x95\xB5" },
    { "disappointed", "\xF0\x9F\x98\x9E" },
    { "dizzy", "\xF0\x9F\x92\xAB" },
    { "dizzy_face", "\xF0\x9F\x98\xB5" },
    { "dna", "\xF0\x9F\xA7\xAC" },
    { "dog", "\xF0\x9F\x90\xB6" },
    { "dollar", "\xF0\x9F\x92\xB5" },
    { "dolphin", "\xF0\x9F\x90\xAC" },
    { "doughnut", "\xF0\x9F\x8D\xA9" },
    { "dragon", "\xF0\x9F\x90\x89" },
    { "drooling_face", "\xF0\x9F\xA4\xA4" },
    { "droplet", "\xF0\x9F\x92\xA7" },
    { "duck", "\xF0\x9F\xA6\x86" },
    { "eagle", "\xF0\x9F\xA6\x85" },
    { "earth_africa", "\xF0\x9F\x8C\x8D" },
    { "earth_americas", "\xF0\x9F\x8C\x8E" },
    { "earth_asia", "\xF0\x9F\x8C\x8F" },
    { "egg", "\xF0\x9F\xA5\x9A" },
    { "eggplant", "\xF0\x9F\x8D\x86" },
    { "eight", "\x38\xEF\xB8\x8F\xE2\x83\xA3" },
    { "eject", "\xE2\x8F\x8F" },
    { "electric_plug", "\xF0\x9F\x94\x8C" },
    { "email", "\xF0\x9F\x93\xA7" },
    { "envelope", "\xE2\x9C\x89" },
    { "evergreen_tree", "\xF0\x9F\x8C\xB2" },
    { "exclamation", "\xE2\x9D\x97" },
    { "exploding_head", "\xF0\x9F\xA4\xAF" },
    { "expressionless", "\xF0\x9F\x98\x91" },
    { "eye", "\xF0\x9F\x91\x81" },
    { "eyes", "\xF0\x9F\x91\x80" },
    { "face_holding_back_tears", "\xF0\x9F\xA5\xB9" },
    { "facepalm", "\xF0\x9F\xA4\xA6" },
    { "fallen_leaf", "\xF0\x9F\x8D\x82" },
    { "fast_forward", "\xE2\x8F\xA9" },
    { "film_frames", "\xF0\x9F\x8E\x9E" },
    { "fire", "\xF0\x9F\x94\xA5" },
    { "first_place", "\xF0\x9F\xA5\x87" },
    { "fish", "\xF0\x9F\x90\x9F" },
    { "fist", "\xE2\x9C\x8A" },
    { "five", "\x35\xEF\xB8\x8F\xE2\x83\xA3" },
    { "flag_br", "\xF0\x9F\x87\xA7\xF0\x9F\x87\xB7" },
    { "flag_ca", "\xF0\x9F\x87\xA8\xF0\x9F\x87\xA6" },
    { "flag_de", "\xF0\x9F\x87\xA9\xF0\x9F\x87\xAA" },
    { "flag_fr", "\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7" },
    { "flag_gb", "\xF0\x9F\x87\xAC\xF0\x9F\x87\xA7" },
    { "flag_in", "\xF0\x9F\x87\xAE\xF0\x9F\x87\xB3" },
    { "flag_jp", "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5" },
    { "flag_mx", "\xF0\x9F\x87\xB2\xF0\x9F\x87\xBD" },
    { "flag_us", "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8" },
    { "flashlight", "\xF0\x9F\x94\xA6" },
    { "flushed", "\xF0\x9F\x98\xB3" },
    { "fog", "\xF0\x9F\x8C\xAB" },
    { "football", "\xF0\x9F\x8F\x88" },
    { "four", "\x34\xEF\xB8\x8F\xE2\x83\xA3" },
    { "four_leaf_clover", "\xF0\x9F\x8D\x80" },
    { "fox", "\xF0\x9F\xA6\x8A" },
    { "free", "\xF0\x9F\x86\x93" },
    { "fries", "\xF0\x9F\x8D\x9F" },
    { "frog", "\xF0\x9F\x90\xB8" },
    { "frowning", "\xF0\x9F\x98\xA6" },
    { "full_moon", "\xF0\x9F\x8C\x95" },
    { "game_die", "\xF0\x9F\x8E\xB2" },
    { "gear", "\xE2\x9A\x99" },
    { "gem", "\xF0\x9F\x92\x8E" },
    { "ghost", "\xF0\x9F\x91\xBB" },
    { "ghost_face", "\xF0\x9F\x91\xBB" },
    { "gift", "\xF0\x9F\x8E\x81" },
    { "gift_heart", "\xF0\x9F\x92\x9D" },
    { "goat", "\xF0\x9F\x90\x90" },
    { "grapes", "\xF0\x9F\x8D\x87" },
    { "green_apple", "\xF0\x9F\x8D\x8F" },
    { "green_circle", "\xF0\x9F\x9F\xA2" },
    { "green_heart", "\xF0\x9F\x92\x9A" },
    { "green_square", "\xF0\x9F\x9F\xA9" },
    { "grimacing", "\xF0\x9F\x98\xAC" },
    { "grin", "\xF0\x9F\x98\x81" },
    { "grinning", "\xF0\x9F\x98\x80" },
    { "guitar", "\xF0\x9F\x8E\xB8" },
    { "hamburger", "\xF0\x9F\x8D\x94" },
    { "hammer", "\xF0\x9F\x94\xA8" },
    { "handshake", "\xF0\x9F\xA4\x9D" },
    { "hash", "\x23\xEF\xB8\x8F\xE2\x83\xA3" },
    { "headphones", "\xF0\x9F\x8E\xA7" },
    { "hear_no_evil", "\xF0\x9F\x99\x89" },
    { "heart", "\xE2\x9D\xA4" },
    { "heart_eyes", "\xF0\x9F\x98\x8D" },
    { "heart_eyes_cat", "\xF0\x9F\x98\xBB" },
    { "heartbeat", "\xF0\x9F\x92\x93" },
    { "heartpulse", "\xF0\x9F\x92\x97" },
    { "heavy_check_mark", "\xE2\x9C\x94" },
    { "heavy_minus_sign", "\xE2\x9E\x96" },
    { "heavy_plus_sign", "\xE2\x9E\x95" },
    { "hedgehog", "\xF0\x9F\xA6\x94" },
    { "herb", "\xF0\x9F\x8C\xBF" },
    { "horse", "\xF0\x9F\x90\xB4" },
    { "hospital", "\xF0\x9F\x8F\xA5" },
    { "hot_face", "\xF0\x9F\xA5\xB5" },
    { "hot_pepper", "\xF0\x9F\x8C\xB6" },
    { "hot_springs", "\xE2\x99\xA8" },
    { "hotdog", "\xF0\x9F\x8C\xAD" },
    { "hourglass", "\xE2\x8C\x9B" },
    { "hourglass_flowing_sand", "\xE2\x8F\xB3" },
    { "house", "\xF0\x9F\x8F\xA0" },
    { "hugging", "\xF0\x9F\xA4\x97" },
    { "hushed", "\xF0\x9F\x98\xAF" },
    { "icecream", "\xF0\x9F\x8D\xA6" },
    { "imp", "\xF0\x9F\x91\xBF" },
    { "infinity", "\xE2\x99\xBE" },
    { "innocent", "\xF0\x9F\x98\x87" },
    { "interrobang", "\xE2\x81\x89" },
    { "iphone", "\xF0\x9F\x93\xB1" },
    { "jack_o_lantern", "\xF0\x9F\x8E\x83" },
    { "joy", "\xF0\x9F\x98\x82" },
    { "joystick", "\xF0\x9F\x95\xB9" },
    { "key", "\xF0\x9F\x94\x91" },
    { "keyboard", "\xE2\x8C\xA8" },
    { "keycap_ten", "\xF0\x9F\x94\x9F" },
    { "kiss", "\xF0\x9F\x92\x8B" },
    { "kissing_heart", "\xF0\x9F\x98\x98" },
    { "koala", "\xF0\x9F\x90\xA8" },
    { "label", "\xF0\x9F\x8F\xB7" },
    { "laughing", "\xF0\x9F\x98\x86" },
    { "leaves", "\xF0\x9F\x8D\x83" },
    { "left_speech_bubble", "\xF0\x9F\x97\xA8" },
    { "lemon", "\xF0\x9F\x8D\x8B" },
    { "link", "\xF0\x9F\x94\x97" },
    { "lion", "\xF0\x9F\xA6\x81" },
    { "lips", "\xF0\x9F\x91\x84" },
    { "llama", "\xF0\x9F\xA6\x99" },
    { "lock", "\xF0\x9F\x94\x92" },
    { "loud_sound", "\xF0\x9F\x94\x8A" },
    { "loudspeaker", "\xF0\x9F\x93\xA2" },
    { "lying_face", "\xF0\x9F\xA4\xA5" },
    { "mag", "\xF0\x9F\x94\x8D" },
    { "mage", "\xF0\x9F\xA7\x99" },
    { "magnet", "\xF0\x9F\xA7\xB2" },
    { "man", "\xF0\x9F\x91\xA8" },
    { "man_dancing", "\xF0\x9F\x95\xBA" },
    { "man_shrugging", "\xF0\x9F\xA4\xB7\xE2\x80\x8D\xE2\x99\x82\xEF\xB8\x8F" },
    { "maple_leaf", "\xF0\x9F\x8D\x81" },
    { "mask", "\xF0\x9F\x98\xB7" },
    { "medal", "\xF0\x9F\x8F\x85" },
    { "mega", "\xF0\x9F\x93\xA3" },
    { "melting_face", "\xF0\x9F\xAB\xA0" },
    { "memo", "\xF0\x9F\x93\x9D" },
    { "metal", "\xF0\x9F\xA4\x98" },
    { "microphone", "\xF0\x9F\x8E\xA4" },
    { "microscope", "\xF0\x9F\x94\xAC" },
    { "milk", "\xF0\x9F\xA5\x9B" },
    { "milky_way", "\xF0\x9F\x8C\x8C" },
    { "money_mouth", "\xF0\x9F\xA4\x91" },
    { "money_with_wings", "\xF0\x9F\x92\xB8" },
    { "moneybag", "\xF0\x9F\x92\xB0" },
    { "monkey", "\xF0\x9F\x90\xB5" },
    { "moon", "\xF0\x9F\x8C\x99" },
    { "mountain", "\xE2\x9B\xB0" },
    { "mouse", "\xF0\x9F\x90\xAD" },
    { "moyai", "\xF0\x9F\x97\xBF" },
    { "muscle", "\xF0\x9F\x92\xAA" },
    { "mushroom", "\xF0\x9F\x8D\x84" },
    { "musical_note", "\xF0\x9F\x8E\xB5" },
    { "mute", "\xF0\x9F\x94\x87" },
    { "nauseated_face", "\xF0\x9F\xA4\xA2" },
    { "nazar_amulet", "\xF0\x9F\xA7\xBF" },
    { "nerd", "\xF0\x9F\xA4\x93" },
    { "neutral_face", "\xF0\x9F\x98\x90" },
    { "new", "\xF0\x9F\x86\x95" },
    { "new_moon", "\xF0\x9F\x8C\x91" },
    { "nine", "\x39\xEF\xB8\x8F\xE2\x83\xA3" },
    { "ninja", "\xF0\x9F\xA5\xB7" },
    { "no_bell", "\xF0\x9F\x94\x95" },
    { "no_entry", "\xE2\x9B\x94" },
    { "no_mouth", "\xF0\x9F\x98\xB6" },
    { "notes", "\xF0\x9F\x8E\xB6" },
    { "o", "\xE2\xAD\x95" },
    { "ocean", "\xF0\x9F\x8C\x8A" },
    { "octopus", "\xF0\x9F\x90\x99" },
    { "office", "\xF0\x9F\x8F\xA2" },
    { "ok", "\xF0\x9F\x86\x97" },
    { "ok_hand", "\xF0\x9F\x91\x8C" },
    { "one", "\x31\xEF\xB8\x8F\xE2\x83\xA3" },
    { "open_hands", "\xF0\x9F\x91\x90" },
    { "open_mouth", "\xF0\x9F\x98\xAE" },
    { "orange_circle", "\xF0\x9F\x9F\xA0" },
    { "orange_heart", "\xF0\x9F\xA7\xA1" },
    { "otter", "\xF0\x9F\xA6\xA6" },
    { "owl", "\xF0\x9F\xA6\x89" },
    { "package", "\xF0\x9F\x93\xA6" },
    { "palm_tree", "\xF0\x9F\x8C\xB4" },
    { "panda", "\xF0\x9F\x90\xBC" },
    { "paperclip", "\xF0\x9F\x93\x8E" },
    { "partying_face", "\xF0\x9F\xA5\xB3" },
    { "pause_button", "\xE2\x8F\xB8" },
    { "peace", "\xE2\x98\xAE" },
    { "peach", "\xF0\x9F\x8D\x91" },
    { "pen", "\xF0\x9F\x96\x8A" },
    { "pencil", "\xF0\x9F\x93\x9D" },
    { "pencil2", "\xE2\x9C\x8F" },
    { "penguin", "\xF0\x9F\x90\xA7" },
    { "pensive", "\xF0\x9F\x98\x94" },
    { "performing_arts", "\xF0\x9F\x8E\xAD" },
    { "persevere", "\xF0\x9F\x98\xA3" },
    { "person", "\xF0\x9F\xA7\x91" },
    { "phone", "\xE2\x98\x8E" },
    { "pig", "\xF0\x9F\x90\xB7" },
    { "pill", "\xF0\x9F\x92\x8A" },
    { "pinched_fingers", "\xF0\x9F\xA4\x8C" },
    { "pirate_flag", "\xF0\x9F\x8F\xB4\xE2\x80\x8D\xE2\x98\xA0\xEF\xB8\x8F" },
    { "pizza", "\xF0\x9F\x8D\x95" },
    { "play_pause", "\xE2\x8F\xAF" },
    { "pleading_face", "\xF0\x9F\xA5\xBA" },
    { "point_down", "\xF0\x9F\x91\x87" },
    { "point_left", "\xF0\x9F\x91\x88" },
    { "point_right", "\xF0\x9F\x91\x89" },
    { "point_up", "\xE2\x98\x9D" },
    { "poop", "\xF0\x9F\x92\xA9" },
    { "popcorn", "\xF0\x9F\x8D\xBF" },
    { "pray", "\xF0\x9F\x99\x8F" },
    { "pumpkin", "\xF0\x9F\x8E\x83" },
    { "punch", "\xF0\x9F\x91\x8A" },
    { "purple_circle", "\xF0\x9F\x9F\xA3" },
    { "purple_heart", "\xF0\x9F\x92\x9C" },
    { "pushpin", "\xF0\x9F\x93\x8C" },
    { "question", "\xE2\x9D\x93" },
    { "rabbit", "\xF0\x9F\x90\xB0" },
    { "rage", "\xF0\x9F\x98\xA1" },
    { "rainbow", "\xF0\x9F\x8C\x88" },
    { "rainbow_flag", "\xF0\x9F\x8F\xB3\xEF\xB8\x8F\xE2\x80\x8D\xF0\x9F\x8C\x88" },
    { "raised_eyebrow", "\xF0\x9F\xA4\xA8" },
    { "raised_hand", "\xE2\x9C\x8B" },
    { "raised_hands", "\xF0\x9F\x99\x8C" },
    { "ramen", "\xF0\x9F\x8D\x9C" },
    { "record_button", "\xE2\x8F\xBA" },
    { "recycle", "\xE2\x99\xBB" },
    { "red_circle", "\xF0\x9F\x94\xB4" },
    { "red_square", "\xF0\x9F\x9F\xA5" },
    { "registered", "\xC2\xAE" },
    { "relieved", "\xF0\x9F\x98\x8C" },
    { "repeat", "\xF0\x9F\x94\x81" },
    { "revolving_hearts", "\xF0\x9F\x92\x9E" },
    { "rewind", "\xE2\x8F\xAA" },
    { "right_anger_bubble", "\xF0\x9F\x97\xAF" },
    { "ring", "\xF0\x9F\x92\x8D" },
    { "robot", "\xF0\x9F\xA4\x96" },
    { "rocket", "\xF0\x9F\x9A\x80" },
    { "rofl", "\xF0\x9F\xA4\xA3" },
    { "rolling_eyes", "\xF0\x9F\x99\x84" },
    { "rose", "\xF0\x9F\x8C\xB9" },
    { "runner", "\xF0\x9F\x8F\x83" },
    { "salute", "\xF0\x9F\xAB\xA1" },
    { "santa", "\xF0\x9F\x8E\x85" },
    { "satellite", "\xF0\x9F\x93\xA1" },
    { "satisfied", "\xF0\x9F\x98\x86" },
    { "sauropod", "\xF0\x9F\xA6\x95" },
    { "school", "\xF0\x9F\x8F\xAB" },
    { "scissors", "\xE2\x9C\x82" },
    { "scream", "\xF0\x9F\x98\xB1" },
    { "see_no_evil", "\xF0\x9F\x99\x88" },
    { "seedling", "\xF0\x9F\x8C\xB1" },
    { "seven", "\x37\xEF\xB8\x8F\xE2\x83\xA3" },
    { "shark", "\xF0\x9F\xA6\x88" },
    { "sheep", "\xF0\x9F\x90\x91" },
    { "shrug", "\xF0\x9F\xA4\xB7" },
    { "shuffle", "\xF0\x9F\x94\x80" },
    { "six", "\x36\xEF\xB8\x8F\xE2\x83\xA3" },
    { "skull", "\xF0\x9F\x92\x80" },
    { "skull_and_crossbones", "\xE2\x98\xA0" },
    { "sleeping", "\xF0\x9F\x98\xB4" },
    { "sleepy", "\xF0\x9F\x98\xAA" },
    { "slight_smile", "\xF0\x9F\x99\x82" },
    { "sloth", "\xF0\x9F\xA6\xA5" },
    { "smile", "\xF0\x9F\x98\x84" },
    { "smiley", "\xF0\x9F\x98\x83" },
    { "smiley_cat", "\xF0\x9F\x98\xBA" },
    { "smiling_imp", "\xF0\x9F\x98\x88" },
    { "smirk", "\xF0\x9F\x98\x8F" },
    { "snail", "\xF0\x9F\x90\x8C" },
    { "snake", "\xF0\x9F\x90\x8D" },
    { "sneezing_face", "\xF0\x9F\xA4\xA7" },
    { "snowflake", "\xE2\x9D\x84" },
    { "snowman", "\xE2\x9B\x84" },
    { "sob", "\xF0\x9F\x98\xAD" },
    { "soccer", "\xE2\x9A\xBD" },
    { "sos", "\xF0\x9F\x86\x98" },
    { "sound", "\xF0\x9F\x94\x89" },
    { "spaghetti", "\xF0\x9F\x8D\x9D" },
    { "sparkles", "\xE2\x9C\xA8" },
    { "sparkling_heart", "\xF0\x9F\x92\x96" },
    { "speak_no_evil", "\xF0\x9F\x99\x8A" },
    { "speaking_head", "\xF0\x9F\x97\xA3" },
    { "speech_balloon", "\xF0\x9F\x92\xAC" },
    { "star", "\xE2\xAD\x90" },
    { "star2", "\xF0\x9F\x8C\x9F" },
    { "star_struck", "\xF0\x9F\xA4\xA9" },
    { "stop_button", "\xE2\x8F\xB9" },
    { "stopwatch", "\xE2\x8F\xB1" },
    { "strawberry", "\xF0\x9F\x8D\x93" },
    { "stuck_out_tongue", "\xF0\x9F\x98\x9B" },
    { "stuck_out_tongue_winking_eye", "\xF0\x9F\x98\x9C" },
    { "sun_with_face", "\xF0\x9F\x8C\x9E" },
    { "sunflower", "\xF0\x9F\x8C\xBB" },
    { "sunglasses", "\xF0\x9F\x98\x8E" },
    { "sunny", "\xE2\x98\x80" },
    { "superhero", "\xF0\x9F\xA6\xB8" },
    { "sushi", "\xF0\x9F\x8D\xA3" },
    { "sweat", "\xF0\x9F\x98\x93" },
    { "sweat_drops", "\xF0\x9F\x92\xA6" },
    { "sweat_smile", "\xF0\x9F\x98\x85" },
    { "syringe", "\xF0\x9F\x92\x89" },
    { "t_rex", "\xF0\x9F\xA6\x96" },
    { "taco", "\xF0\x9F\x8C\xAE" },
    { "tada", "\xF0\x9F\x8E\x89" },
    { "tea", "\xF0\x9F\x8D\xB5" },
    { "telescope", "\xF0\x9F\x94\xAD" },
    { "tennis", "\xF0\x9F\x8E\xBE" },
    { "tent", "\xE2\x9B\xBA" },
    { "test_tube", "\xF0\x9F\xA7\xAA" },
    { "thermometer", "\xF0\x9F\x8C\xA1" },
    { "thinking", "\xF0\x9F\xA4\x94" },
    { "thought_balloon", "\xF0\x9F\x92\xAD" },
    { "three", "\x33\xEF\xB8\x8F\xE2\x83\xA3" },
    { "thumbsdown", "\xF0\x9F\x91\x8E" },
    { "thumbsup", "\xF0\x9F\x91\x8D" },
    { "thunder_cloud_and_rain", "\xE2\x9B\x88" },
    { "ticket", "\xF0\x9F\x8E\xAB" },
    { "tiger", "\xF0\x9F\x90\xAF" },
    { "tired_face", "\xF0\x9F\x98\xAB" },
    { "tm", "\xE2\x84\xA2" },
    { "tongue", "\xF0\x9F\x91\x85" },
    { "tornado", "\xF0\x9F\x8C\xAA" },
    { "trash", "\xF0\x9F\x97\x91" },
    { "triangular_flag", "\xF0\x9F\x9A\xA9" },
    { "triumph", "\xF0\x9F\x98\xA4" },
    { "trophy", "\xF0\x9F\x8F\x86" },
    { "tulip", "\xF0\x9F\x8C\xB7" },
    { "turtle", "\xF0\x9F\x90\xA2" },
    { "tv", "\xF0\x9F\x93\xBA" },
    { "two", "\x32\xEF\xB8\x8F\xE2\x83\xA3" },
    { "two_hearts", "\xF0\x9F\x92\x95" },
    { "umbrella", "\xE2\x98\x94" },
    { "unamused", "\xF0\x9F\x98\x92" },
    { "unicorn", "\xF0\x9F\xA6\x84" },
    { "unlock", "\xF0\x9F\x94\x93" },
    { "up", "\xF0\x9F\x86\x99" },
    { "upside_down", "\xF0\x9F\x99\x83" },
    { "v", "\xE2\x9C\x8C" },
    { "vampire", "\xF0\x9F\xA7\x9B" },
    { "video_game", "\xF0\x9F\x8E\xAE" },
    { "volcano", "\xF0\x9F\x8C\x8B" },
    { "vomiting_face", "\xF0\x9F\xA4\xAE" },
    { "vs", "\xF0\x9F\x86\x9A" },
    { "walking", "\xF0\x9F\x9A\xB6" },
    { "warning", "\xE2\x9A\xA0" },
    { "watch", "\xE2\x8C\x9A" },
    { "watermelon", "\xF0\x9F\x8D\x89" },
    { "wave", "\xF0\x9F\x91\x8B" },
    { "weary", "\xF0\x9F\x98\xA9" },
    { "whale", "\xF0\x9F\x90\xB3" },
    { "white_check_mark", "\xE2\x9C\x85" },
    { "white_circle", "\xE2\x9A\xAA" },
    { "white_flag", "\xF0\x9F\x8F\xB3" },
    { "white_heart", "\xF0\x9F\xA4\x8D" },
    { "wine_glass", "\xF0\x9F\x8D\xB7" },
    { "wink", "\xF0\x9F\x98\x89" },
    { "wolf", "\xF0\x9F\x90\xBA" },
    { "woman", "\xF0\x9F\x91\xA9" },
    { "woman_shrugging", "\xF0\x9F\xA4\xB7\xE2\x80\x8D\xE2\x99\x80\xEF\xB8\x8F" },
    { "woozy_face", "\xF0\x9F\xA5\xB4" },
    { "world_map", "\xF0\x9F\x97\xBA" },
    { "worried", "\xF0\x9F\x98\x9F" },
    { "wrench", "\xF0\x9F\x94\xA7" },
    { "writing_hand", "\xE2\x9C\x8D" },
    { "x", "\xE2\x9D\x8C" },
    { "yawning_face", "\xF0\x9F\xA5\xB1" },
    { "yellow_circle", "\xF0\x9F\x9F\xA1" },
    { "yellow_heart", "\xF0\x9F\x92\x9B" },
    { "yin_yang", "\xE2\x98\xAF" },
    { "yum", "\xF0\x9F\x98\x8B" },
    { "zany_face", "\xF0\x9F\xA4\xAA" },
    { "zap", "\xE2\x9A\xA1" },
    { "zero", "\x30\xEF\xB8\x8F\xE2\x83\xA3" },
    { "zipper_mouth", "\xF0\x9F\xA4\x90" },
    { "zombie", "\xF0\x9F\xA7\x9F" },
    { "zzz", "\xF0\x9F\x92\xA4" }
};

std::uint32_t decode_utf8(std::string_view s, std::size_t& i) noexcept {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { ++i; return c; }
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    std::uint32_t cp = extra == 3 ? (c & 0x07u) : extra == 2 ? (c & 0x0Fu) : (c & 0x1Fu);
    if (extra == 0) { ++i; return 0xFFFD; }
    ++i;
    while (extra-- > 0) {
        if (i >= s.size() || (static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3Fu);
        ++i;
    }
    return cp;
}

bool is_emoji_codepoint(std::uint32_t cp) noexcept {
    return (cp >= 0x1F000 && cp <= 0x1FAFF) || (cp >= 0x2600 && cp <= 0x27BF) || (cp >= 0x2300 && cp <= 0x23FF) || (cp >= 0x2B00 && cp <= 0x2BFF) ||
           cp == 0x3030 || cp == 0x303D || cp == 0x3297 || cp == 0x3299 || cp == 0x00A9 || cp == 0x00AE || cp == 0x203C || cp == 0x2049 || cp == 0x2122 || cp == 0x2139 ||
           (cp >= 0x2194 && cp <= 0x21AA) || cp == 0x24C2 || (cp >= 0x25AA && cp <= 0x25FE) || (cp >= 0xE0020 && cp <= 0xE007F);
}

bool is_emoji_joiner(std::uint32_t cp) noexcept { return cp == 0x200D || cp == 0xFE0F || cp == 0xFE0E || cp == 0x20E3 || (cp >= 0x1F3FB && cp <= 0x1F3FF); }

std::mutex& clock_mutex() {
    static std::mutex m;
    return m;
}

std::function<double()>& clock_fn() {
    static std::function<double()> f;
    return f;
}

} // namespace

std::string EmojiRegistry::normalize(std::string_view name) {
    std::string n(name);
    if (n.size() >= 2 && n.front() == ':' && n.back() == ':') n = n.substr(1, n.size() - 2);
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return n;
}

void EmojiRegistry::add(std::string name, EmojiImage image) {
    if (!image || image->empty()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_images[normalize(name)] = std::move(image);
}

void EmojiRegistry::add(std::string name, const images::BitmapImage& still) { add(std::move(name), std::make_shared<const images::AnimatedImage>(still)); }

void EmojiRegistry::add(std::string name, images::AnimatedImage animation) { add(std::move(name), std::make_shared<const images::AnimatedImage>(std::move(animation))); }

bool EmojiRegistry::add_file(std::string name, const std::string& path, std::string* error) {
    images::AnimatedImage anim;
    if (!images::load_animation(path, anim, error)) return false;
    add(std::move(name), std::move(anim));
    return true;
}

bool EmojiRegistry::add_memory(std::string name, const std::uint8_t* data, std::size_t size, std::string* error) {
    images::AnimatedImage anim;
    if (!images::decode_animation(data, size, anim, error)) return false;
    add(std::move(name), std::move(anim));
    return true;
}

std::size_t EmojiRegistry::add_directory(const std::string& directory, std::string* error) {
    std::size_t added = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const std::string path = entry.path().string();
        if (images::format_from_extension(path) == images::ImageFormat::Unknown) continue;
        if (add_file(entry.path().stem().string(), path, nullptr)) ++added;
    }
    if (ec && error) *error = ec.message();
    return added;
}

bool EmojiRegistry::remove(std::string_view name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_images.erase(normalize(name)) > 0;
}

void EmojiRegistry::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_images.clear();
}

bool EmojiRegistry::contains(std::string_view name) const { return find(name) != nullptr; }

EmojiImage EmojiRegistry::find(std::string_view name) const {
    const std::string key = normalize(name);
    std::function<EmojiImage(std::string_view)> resolver;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_images.find(key);
        if (it != m_images.end()) return it->second;
        resolver = m_resolver;
    }
    if (!resolver) return nullptr;
    EmojiImage img = resolver(key);
    if (img && !img->empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_images.emplace(key, img);
    }
    return img;
}

std::size_t EmojiRegistry::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_images.size();
}

std::vector<std::string> EmojiRegistry::names() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> out;
    out.reserve(m_images.size());
    for (const auto& kv : m_images) out.push_back(kv.first);
    std::sort(out.begin(), out.end());
    return out;
}

void EmojiRegistry::set_resolver(std::function<EmojiImage(std::string_view)> resolver) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resolver = std::move(resolver);
}

EmojiRegistry& emoji_registry() {
    static EmojiRegistry r;
    return r;
}

std::string_view unicode_emoji(std::string_view shortcode) noexcept {
    if (shortcode.size() >= 2 && shortcode.front() == ':' && shortcode.back() == ':') shortcode = shortcode.substr(1, shortcode.size() - 2);
    const Shortcode* b = std::begin(kShortcodes);
    const Shortcode* e = std::end(kShortcodes);
    const Shortcode* it = std::lower_bound(b, e, shortcode, [](const Shortcode& s, std::string_view k) { return std::string_view(s.name) < k; });
    if (it != e && std::string_view(it->name) == shortcode) return it->utf8;
    return {};
}

std::vector<std::string_view> unicode_emoji_shortcodes() {
    std::vector<std::string_view> out;
    for (const Shortcode& s : kShortcodes) out.emplace_back(s.name);
    return out;
}

bool is_emoji_only(std::string_view utf8) noexcept {
    std::size_t i = 0;
    bool any = false;
    while (i < utf8.size()) {
        const std::uint32_t cp = decode_utf8(utf8, i);
        if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r') continue;
        if (is_emoji_codepoint(cp)) { any = true; continue; }
        if (is_emoji_joiner(cp)) continue;
        if ((cp >= '0' && cp <= '9') || cp == '#' || cp == '*') {
            std::size_t j = i;
            if (j < utf8.size()) {
                std::uint32_t next = decode_utf8(utf8, j);
                if (next == 0xFE0F && j < utf8.size()) next = decode_utf8(utf8, j);
                if (next == 0x20E3) { i = j; any = true; continue; }
            }
        }
        return false;
    }
    return any;
}

double animation_clock_ms() {
    std::function<double()> f;
    {
        std::lock_guard<std::mutex> lock(clock_mutex());
        f = clock_fn();
    }
    if (f) return f();
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

void set_animation_clock(std::function<double()> clock) {
    std::lock_guard<std::mutex> lock(clock_mutex());
    clock_fn() = std::move(clock);
}

void reset_animation_clock() { set_animation_clock(nullptr); }

} // namespace text
} // namespace fizmo
