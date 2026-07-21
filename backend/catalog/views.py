from rest_framework import viewsets
from rest_framework.permissions import AllowAny

from .models import Configuration, Vehicle
from .serializers import ConfigurationSerializer, VehicleDetailSerializer, VehicleListSerializer


class VehicleViewSet(viewsets.ReadOnlyModelViewSet):
    """Browse the catalog (list) and load a single vehicle's full option set (retrieve)."""

    queryset = Vehicle.objects.filter(is_published=True)
    permission_classes = [AllowAny]
    filterset_fields = ["vehicle_type"]
    lookup_field = "slug"

    def get_serializer_class(self):
        if self.action == "retrieve":
            return VehicleDetailSerializer
        return VehicleListSerializer


class ConfigurationViewSet(viewsets.ModelViewSet):
    """Create/update/fetch a saved build. No auth yet — tied to session_key for now."""

    queryset = Configuration.objects.all()
    serializer_class = ConfigurationSerializer
    permission_classes = [AllowAny]

    def get_queryset(self):
        session_key = self.request.query_params.get("session_key")
        if session_key:
            return self.queryset.filter(session_key=session_key)
        return self.queryset
