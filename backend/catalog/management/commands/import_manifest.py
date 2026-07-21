import json
from pathlib import Path

from django.conf import settings
from django.core.management.base import BaseCommand, CommandError
from django.utils.text import slugify

from catalog.models import Vehicle


class Command(BaseCommand):
    """
    Reads manifest.json produced by the Java asset-pipeline and creates/updates
    a Vehicle row for each entry, pointing model_file at the optimized .glb.

    Usage:
        python manage.py import_manifest /path/to/manifest.json
    """

    help = "Import vehicles from an asset-pipeline manifest.json"

    def add_arguments(self, parser):
        parser.add_argument("manifest_path", type=str)
        parser.add_argument(
            "--publish",
            action="store_true",
            help="Mark imported vehicles as published immediately.",
        )

    def handle(self, *args, **options):
        manifest_path = Path(options["manifest_path"])
        if not manifest_path.exists():
            raise CommandError(f"Manifest not found: {manifest_path}")

        entries = json.loads(manifest_path.read_text())["assets"]
        created, updated = 0, 0

        for entry in entries:
            slug = entry.get("slug") or slugify(entry["name"])
            defaults = {
                "name": entry["name"],
                "vehicle_type": entry["vehicleType"],
                "model_file": entry["glbPath"],
                "is_published": options["publish"],
            }
            if "basePrice" in entry:
                defaults["base_price"] = entry["basePrice"]
            else:
                defaults.setdefault("base_price", 0)

            vehicle, was_created = Vehicle.objects.update_or_create(
                slug=slug, defaults=defaults
            )
            created += int(was_created)
            updated += int(not was_created)

        self.stdout.write(
            self.style.SUCCESS(f"Imported manifest: {created} created, {updated} updated.")
        )
