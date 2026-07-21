from rest_framework.routers import DefaultRouter

from .views import ConfigurationViewSet, VehicleViewSet

router = DefaultRouter()
router.register("vehicles", VehicleViewSet, basename="vehicle")
router.register("configurations", ConfigurationViewSet, basename="configuration")

urlpatterns = router.urls
