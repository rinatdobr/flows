-- Create items table to establish hierarchical relationship between flows and their items
CREATE TABLE items (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    flow_id UUID NOT NULL REFERENCES flows(id) ON DELETE CASCADE,
    item_type VARCHAR(50) NOT NULL CHECK (item_type IN ('flow', 'task')),
    item_id UUID NOT NULL,
    order_id INTEGER NOT NULL DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Create indexes for faster queries
CREATE INDEX idx_items_flow_id ON items(flow_id);
CREATE INDEX idx_items_item_type ON items(item_type);
CREATE INDEX idx_items_order_id ON items(order_id);

-- Create unique constraint to prevent duplicate items within a flow
CREATE UNIQUE INDEX idx_items_unique ON items(flow_id, item_type, item_id);
