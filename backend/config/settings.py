"""
Django settings for the Kraft Configurator backend.
"""
import os
from pathlib import Path

from dotenv import load_dotenv

BASE_DIR = Path(__file__).resolve().parent.parent
load_dotenv(BASE_DIR / ".env")

SECRET_KEY = os.environ.get("DJANGO_SECRET_KEY", "insecure-dev-key-change-me")
DEBUG = os.environ.get("DJANGO_DEBUG", "True") == "True"
ALLOWED_HOSTS = os.environ.get("DJANGO_ALLOWED_HOSTS", "localhost,127.0.0.1").split(",")

INSTALLED_APPS = [
    "jazzmin",
    "django.contrib.admin",
    "django.contrib.auth",
    "django.contrib.contenttypes",
    "django.contrib.sessions",
    "django.contrib.messages",
    "django.contrib.staticfiles",
    # third party
    "rest_framework",
    "django_filters",
    "corsheaders",
    # local
    "catalog",
]

JAZZMIN_SETTINGS = {
    # ── Branding ──────────────────────────────────────────────────────────────
    "site_title": "Kraft Configurator",
    "site_header": "Kraft Configurator",
    "site_brand": "Kraft",
    "site_icon": "favicon.ico",
    "welcome_sign": "Welcome to the Kraft Configurator Admin",
    "copyright": "Kraft Configurator",

    # ── Top navigation links ──────────────────────────────────────────────────
    "topmenu_links": [
        {"name": "Home", "url": "admin:index", "permissions": ["auth.view_user"]},
        {"name": "API", "url": "/api/", "new_window": True},
    ],

    # ── Sidebar icons (Font Awesome 5) ────────────────────────────────────────
    "icons": {
        "auth":                     "fas fa-users-cog",
        "auth.user":                "fas fa-user",
        "auth.Group":               "fas fa-users",
        "catalog.Vehicle":          "fas fa-car",
        "catalog.OptionCategory":   "fas fa-list-ul",
        "catalog.OptionChoice":     "fas fa-sliders-h",
        "catalog.Configuration":    "fas fa-cogs",
        "catalog.ConfigurationSelection": "fas fa-check-square",
    },
    "default_icon_parents": "fas fa-folder",
    "default_icon_children": "fas fa-circle",

    # ── UI Tweaks ─────────────────────────────────────────────────────────────
    "show_sidebar": True,
    "navigation_expanded": True,
    "hide_apps": [],
    "hide_models": [],
    "order_with_respect_to": ["auth", "catalog"],

    # ── Search bar ────────────────────────────────────────────────────────────
    "search_model": ["catalog.Vehicle", "auth.User"],

    # ── Related modal (open FK lookups in a popup) ────────────────────────────
    "related_modal_active": True,

    # ── Language chooser & theme ──────────────────────────────────────────────
    "show_ui_builder": False,
    "changeform_format": "horizontal_tabs",
    "changeform_format_overrides": {
        "auth.user": "collapsible",
        "auth.group": "vertical_tabs",
    },
}

JAZZMIN_UI_TWEAKS = {
    "navbar_small_text": False,
    "footer_small_text": False,
    "body_small_text": False,
    "brand_small_text": False,
    "brand_colour": "navbar-dark",
    "accent": "accent-primary",
    "navbar": "navbar-dark",
    "no_navbar_border": True,
    "navbar_fixed": True,
    "layout_boxed": False,
    "footer_fixed": False,
    "sidebar_fixed": True,
    "sidebar": "sidebar-dark-primary",
    "sidebar_nav_small_text": False,
    "sidebar_disable_expand": False,
    "sidebar_nav_child_indent": True,
    "sidebar_nav_compact_style": False,
    "sidebar_nav_legacy_style": False,
    "sidebar_nav_flat_style": False,
    "theme": "flatly",
    "dark_mode_theme": "flatly",
    "button_classes": {
        "primary": "btn-primary",
        "secondary": "btn-secondary",
        "info": "btn-info",
        "warning": "btn-warning",
        "danger": "btn-danger",
        "success": "btn-success",
    },
}

MIDDLEWARE = [
    "django.middleware.security.SecurityMiddleware",
    "corsheaders.middleware.CorsMiddleware",
    "django.contrib.sessions.middleware.SessionMiddleware",
    "django.middleware.common.CommonMiddleware",
    "django.middleware.csrf.CsrfViewMiddleware",
    "django.contrib.auth.middleware.AuthenticationMiddleware",
    "django.contrib.messages.middleware.MessageMiddleware",
    "django.middleware.clickjacking.XFrameOptionsMiddleware",
]

ROOT_URLCONF = "config.urls"

TEMPLATES = [
    {
        "BACKEND": "django.template.backends.django.DjangoTemplates",
        "DIRS": [],
        "APP_DIRS": True,
        "OPTIONS": {
            "context_processors": [
                "django.template.context_processors.debug",
                "django.template.context_processors.request",
                "django.contrib.auth.context_processors.auth",
                "django.contrib.messages.context_processors.messages",
            ],
        },
    },
]

WSGI_APPLICATION = "config.wsgi.application"
ASGI_APPLICATION = "config.asgi.application"

# Plain SQLite for local dev — swap for Postgres in production
# (DATABASE_URL-style env parsing can be added with dj-database-url if you'd rather).
DATABASES = {
    "default": {
        "ENGINE": "django.db.backends.sqlite3",
        "NAME": BASE_DIR / "db.sqlite3",
    }
}

AUTH_PASSWORD_VALIDATORS = [
    {"NAME": "django.contrib.auth.password_validation.UserAttributeSimilarityValidator"},
    {"NAME": "django.contrib.auth.password_validation.MinimumLengthValidator"},
    {"NAME": "django.contrib.auth.password_validation.CommonPasswordValidator"},
    {"NAME": "django.contrib.auth.password_validation.NumericPasswordValidator"},
]

LANGUAGE_CODE = "en-us"
TIME_ZONE = "UTC"
USE_I18N = True
USE_TZ = True

STATIC_URL = "static/"
STATIC_ROOT = BASE_DIR / "staticfiles"
# Project-level static assets (favicon, etc.) that aren't tied to a specific
# app. Anything dropped in here is served at /static/<filename> in dev, and
# picked up by `collectstatic` for production.
STATICFILES_DIRS = [BASE_DIR / "static"]

MEDIA_URL = "/media/"
MEDIA_ROOT = BASE_DIR / "media"

DEFAULT_AUTO_FIELD = "django.db.models.BigAutoField"

REST_FRAMEWORK = {
    "DEFAULT_FILTER_BACKENDS": ["django_filters.rest_framework.DjangoFilterBackend"],
    "DEFAULT_PAGINATION_CLASS": "rest_framework.pagination.PageNumberPagination",
    "PAGE_SIZE": 20,
}

CORS_ALLOWED_ORIGINS = os.environ.get(
    "CORS_ALLOWED_ORIGINS", "http://localhost:5173"
).split(",")