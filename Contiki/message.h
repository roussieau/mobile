enum {
	BROADCAST_TYPE_DISCOVER,
	BROADCAST_TYPE_CONFIG,
	BROADCAST_TYPE_SIGNALLOST,
};

struct broadcast_msg {
	uint8_t type;
	int16_t info;
};


