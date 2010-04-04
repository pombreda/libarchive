from _extension import archive_stream, archive_entry


class Archive(object):

    @classmethod
    def from_filepath(kls, filepath):
        return kls(archive_stream(filepath))

    @classmethod
    def from_stream(kls, stream):
        return kls(stream)

    def __init__(self, obj):
        if not isinstance(obj, archive_stream):
            obj = archive_stream(obj)

        self._items = []
        self._stream = obj

    def __iter__(self):
        for entry in self._items:
            yield entry
        for entry in self._stream:
            self._items.append(entry)
            yield entry


def open(filepath):
    return Archive.from_filepath(filepath)
