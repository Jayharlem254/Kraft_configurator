from rest_framework import serializers

from .models import Configuration, ConfigurationSelection, OptionCategory, OptionChoice, Vehicle


class RelativeFileField(serializers.FileField):
    """FileField/ImageField that always serializes to a relative URL
    (e.g. "/media/models/velora-gt.glb") instead of DRF's default behavior of
    building an absolute URI from the request (e.g. "http://localhost:8000/media/...").

    The absolute-URL default is actively harmful here: the frontend dev server
    runs on a different port (5173/3000) than Django (8000), so an absolute
    URL turns every model/thumbnail fetch into a cross-origin request subject
    to CORS — and it'll keep breaking every time the dev port changes, or in
    prod behind a reverse proxy. A relative URL is always same-origin from the
    browser's perspective, so Vite's dev proxy (or a prod reverse proxy)
    handles it transparently instead.
    """

    def to_representation(self, value):
        if not value:
            return None
        try:
            return value.url
        except AttributeError:
            return None


class OptionChoiceSerializer(serializers.ModelSerializer):
    thumbnail = RelativeFileField(read_only=True)

    class Meta:
        model = OptionChoice
        fields = [
            "id",
            "name",
            "price_delta",
            "swatch_hex",
            "thumbnail",
            "mesh_target",
            "is_default",
        ]


class OptionCategorySerializer(serializers.ModelSerializer):
    choices = OptionChoiceSerializer(many=True, read_only=True)

    class Meta:
        model = OptionCategory
        fields = ["id", "name", "slug", "selection_type", "choices"]


class VehicleListSerializer(serializers.ModelSerializer):
    """Lightweight serializer for catalog/browse views."""

    thumbnail = RelativeFileField(read_only=True)

    class Meta:
        model = Vehicle
        fields = [
            "id",
            "name",
            "slug",
            "vehicle_type",
            "tagline",
            "base_price",
            "thumbnail",
        ]


class VehicleDetailSerializer(serializers.ModelSerializer):
    """Full serializer for the configurator screen — includes options + model file."""

    option_categories = OptionCategorySerializer(many=True, read_only=True)
    model_file = RelativeFileField(read_only=True)
    thumbnail = RelativeFileField(read_only=True)

    class Meta:
        model = Vehicle
        fields = [
            "id",
            "name",
            "slug",
            "vehicle_type",
            "tagline",
            "description",
            "base_price",
            "model_file",
            "thumbnail",
            "option_categories",
        ]


class ConfigurationSelectionSerializer(serializers.ModelSerializer):
    option = OptionChoiceSerializer(read_only=True)

    class Meta:
        model = ConfigurationSelection
        fields = ["option"]


class ConfigurationSerializer(serializers.ModelSerializer):
    selections = ConfigurationSelectionSerializer(many=True, read_only=True)
    total_price = serializers.DecimalField(max_digits=10, decimal_places=2, read_only=True)
    option_ids = serializers.PrimaryKeyRelatedField(
        queryset=OptionChoice.objects.all(), many=True, write_only=True, source="selected_options"
    )

    class Meta:
        model = Configuration
        fields = [
            "id",
            "vehicle",
            "name",
            "session_key",
            "selections",
            "total_price",
            "option_ids",
            "created_at",
            "updated_at",
        ]
        read_only_fields = ["id", "created_at", "updated_at"]

    def create(self, validated_data):
        options = validated_data.pop("selected_options")
        configuration = Configuration.objects.create(**validated_data)
        configuration.selected_options.set(options)
        return configuration

    def update(self, instance, validated_data):
        options = validated_data.pop("selected_options", None)
        for attr, value in validated_data.items():
            setattr(instance, attr, value)
        instance.save()
        if options is not None:
            instance.selected_options.set(options)
        return instance