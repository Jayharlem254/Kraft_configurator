from django.contrib import admin

from .models import Configuration, ConfigurationSelection, OptionCategory, OptionChoice, Vehicle


class OptionChoiceInline(admin.TabularInline):
    model = OptionChoice
    extra = 1


class OptionCategoryInline(admin.TabularInline):
    model = OptionCategory
    extra = 1
    show_change_link = True


@admin.register(Vehicle)
class VehicleAdmin(admin.ModelAdmin):
    list_display = ["name", "vehicle_type", "base_price", "is_published"]
    list_filter = ["vehicle_type", "is_published"]
    search_fields = ["name", "slug"]
    inlines = [OptionCategoryInline]


@admin.register(OptionCategory)
class OptionCategoryAdmin(admin.ModelAdmin):
    list_display = ["name", "vehicle", "selection_type", "display_order"]
    list_filter = ["vehicle"]
    inlines = [OptionChoiceInline]


class ConfigurationSelectionInline(admin.TabularInline):
    model = ConfigurationSelection
    extra = 0


@admin.register(Configuration)
class ConfigurationAdmin(admin.ModelAdmin):
    list_display = ["id", "vehicle", "name", "created_at"]
    inlines = [ConfigurationSelectionInline]
