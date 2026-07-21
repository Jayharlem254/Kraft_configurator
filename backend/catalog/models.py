import uuid

from django.core.validators import MinValueValidator
from django.db import models


class VehicleType(models.TextChoices):
    CAR = "car", "Car"
    BOAT = "boat", "Speed boat"
    BIKE = "bike", "Bike"


class Vehicle(models.Model):
    """A single configurable vehicle (e.g. 'Velora GT', a car model)."""

    name = models.CharField(max_length=120)
    slug = models.SlugField(max_length=140, unique=True)
    vehicle_type = models.CharField(max_length=10, choices=VehicleType.choices)
    tagline = models.CharField(max_length=200, blank=True)
    description = models.TextField(blank=True)
    base_price = models.DecimalField(
        max_digits=10, decimal_places=2, validators=[MinValueValidator(0)]
    )

    # Path to the optimized .glb produced by the asset pipeline, relative to MEDIA_ROOT.
    model_file = models.FileField(upload_to="models/")
    thumbnail = models.ImageField(upload_to="thumbnails/", blank=True, null=True)

    is_published = models.BooleanField(default=False)
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    class Meta:
        ordering = ["vehicle_type", "name"]

    def __str__(self):
        return f"{self.name} ({self.get_vehicle_type_display()})"


class OptionCategory(models.Model):
    """A group of choices for one vehicle, e.g. 'Exterior color' or 'Wheels'."""

    class SelectionType(models.TextChoices):
        SINGLE = "single", "Pick one"
        MULTIPLE = "multiple", "Pick any"

    vehicle = models.ForeignKey(
        Vehicle, on_delete=models.CASCADE, related_name="option_categories"
    )
    name = models.CharField(max_length=120)
    slug = models.SlugField(max_length=140)
    selection_type = models.CharField(
        max_length=10, choices=SelectionType.choices, default=SelectionType.SINGLE
    )
    display_order = models.PositiveSmallIntegerField(default=0)

    class Meta:
        ordering = ["display_order", "name"]
        unique_together = ("vehicle", "slug")
        verbose_name_plural = "Option categories"

    def __str__(self):
        return f"{self.vehicle.name} — {self.name}"


class OptionChoice(models.Model):
    """One selectable choice within a category, e.g. 'Nebula Purple' paint."""

    category = models.ForeignKey(
        OptionCategory, on_delete=models.CASCADE, related_name="choices"
    )
    name = models.CharField(max_length=120)
    price_delta = models.DecimalField(max_digits=10, decimal_places=2, default=0)

    # For color/finish swatches in the UI.
    swatch_hex = models.CharField(max_length=7, blank=True, help_text="e.g. #6d28d9")
    thumbnail = models.ImageField(upload_to="option_thumbnails/", blank=True, null=True)

    # Identifies which mesh/material node inside the .glb this choice applies to,
    # so the frontend knows what to swap (e.g. "Body_Paint", "Wheel_Rim").
    mesh_target = models.CharField(max_length=120, blank=True)

    is_default = models.BooleanField(default=False)
    display_order = models.PositiveSmallIntegerField(default=0)

    class Meta:
        ordering = ["display_order", "name"]

    def __str__(self):
        return f"{self.category.name} — {self.name}"


class Configuration(models.Model):
    """A saved (or in-progress) build of a vehicle: a chosen set of options."""

    id = models.UUIDField(primary_key=True, default=uuid.uuid4, editable=False)
    vehicle = models.ForeignKey(
        Vehicle, on_delete=models.CASCADE, related_name="configurations"
    )
    name = models.CharField(max_length=120, blank=True)
    # Anonymous configs are tied to a browser session; logged-in users can claim one later.
    session_key = models.CharField(max_length=64, blank=True, db_index=True)
    selected_options = models.ManyToManyField(
        OptionChoice, through="ConfigurationSelection", related_name="configurations"
    )
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    @property
    def total_price(self):
        options_total = sum(
            (selection.option.price_delta for selection in self.selections.select_related("option")),
            start=self.vehicle.base_price.__class__(0),
        )
        return self.vehicle.base_price + options_total

    def __str__(self):
        return f"Config {self.id} — {self.vehicle.name}"


class ConfigurationSelection(models.Model):
    """Through-model linking a Configuration to its chosen OptionChoices."""

    configuration = models.ForeignKey(
        Configuration, on_delete=models.CASCADE, related_name="selections"
    )
    option = models.ForeignKey(OptionChoice, on_delete=models.CASCADE)

    class Meta:
        unique_together = ("configuration", "option")
